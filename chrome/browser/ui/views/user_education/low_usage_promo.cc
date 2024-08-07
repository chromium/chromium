// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/low_usage_promo.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using AcceleratorInfo =
    user_education::FeaturePromoSpecification::AcceleratorInfo;
using user_education::FeaturePromoHandle;
using user_education::FeaturePromoSpecification;
using user_education::HelpBubbleArrow;

namespace {

const base::Feature& kLowUsagePromo =
    feature_engagement::kIPHDesktopReEngagementFeature;
constexpr char kLowUsagePromoFocusFieldTrialParam[] = "focus_on_show";

constexpr char kIncludeAiPromosFieldTrialParam[] = "include_ai";

constexpr char kBrowserFeaturesUrl[] =
    "https://www.google.com/chrome/browser-features/";
constexpr char kBrowserToolsUrl[] =
    "https://www.google.com/chrome/browser-tools/";
constexpr char kChromeSecurityBlogUrl[] =
    "https://blog.google/products/chrome/"
    "google-chrome-safe-browsing-real-time/";
constexpr char kExtensionsWebStoreUrl[] =
    "https://chromewebstore.google.com/category/extensions";
constexpr char kGoogleChromeAiUrl[] =
    "https://www.google.com/chrome/ai-innovations";
constexpr char kGoogleChromeSafetyUrl[] = "https://www.google.com/chrome/#safe";
constexpr char kGooglePasswordsUrl[] = "https://passwords.google";
constexpr char kThemesWebStoreUrl[] =
    "https://chromewebstore.google.com/category/themes";

// Convenience method for retrieving a browser from its context.
Browser* ContextToBrowser(ui::ElementContext ctx) {
  Browser* const browser = chrome::FindBrowserWithUiElementContext(ctx);
  if (!browser) {
    NOTREACHED_IN_MIGRATION() << "Promo attempted to open a side panel but the "
                                 "browser context was invalid.";
  }
  return browser;
}

// Convenience method for showing a side panel in a browser; the panel must
// already exist.
void ShowSidePanel(Browser* browser, SidePanelEntryId entry) {
  SidePanelCoordinator* const coordinator =
      browser->GetFeatures().side_panel_coordinator();
  coordinator->Show(entry);
}

// Convenience method for navigating to a page in a new tab; returns the new
// WebContents.
content::WebContents* NavigateToPage(Browser* browser, const GURL& url) {
  NavigateParams navigate_params(browser, GURL(url), ui::PAGE_TRANSITION_TYPED);
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
  if (!navigate_params.navigated_or_inserted_contents) {
    NOTREACHED_IN_MIGRATION()
        << "Promo attempted to open a page, but did not receive a "
           "navigation handle.";
  }
  return navigate_params.navigated_or_inserted_contents;
}

// Self-deleting object that waits for a page to load and then opens a side
// panel. Useful because, for example, the Customize Chrome side panel does not
// exist if the current page isn't the New Tab Page.
class OpenSidePanelOnNavigation : public content::WebContentsObserver {
 public:
  static void Show(content::WebContents* contents, SidePanelEntryId entry) {
    new OpenSidePanelOnNavigation(contents, entry);
  }

 private:
  OpenSidePanelOnNavigation(content::WebContents* contents,
                            SidePanelEntryId side_panel_id)
      : WebContentsObserver(contents), side_panel_id_(side_panel_id) {
    if (contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&OpenSidePanelOnNavigation::
                             DocumentOnLoadCompletedInPrimaryMainFrame,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
  ~OpenSidePanelOnNavigation() override = default;

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    if (Browser* browser = chrome::FindBrowserWithTab(web_contents())) {
      ShowSidePanel(browser, side_panel_id_);
    }
    delete this;
  }

  void WebContentsDestroyed() override { delete this; }

  const SidePanelEntryId side_panel_id_;
  base::WeakPtrFactory<OpenSidePanelOnNavigation> weak_ptr_factory_{this};
};

// Creates a custom action promo that navigates to the specified page.
FeaturePromoSpecification CreateNavigatePromo(int body_text_id,
                                              int action_text_id,
                                              GURL url) {
  auto spec = FeaturePromoSpecification::CreateForCustomAction(
      kLowUsagePromo, kTopContainerElementId, body_text_id, action_text_id,
      base::BindRepeating(
          [](GURL url, ui::ElementContext ctx, FeaturePromoHandle) {
            if (Browser* const browser = ContextToBrowser(ctx)) {
              NavigateToPage(browser, url);
            }
          },
          url));
  spec.SetBubbleArrow(HelpBubbleArrow::kNone);
  return spec;
}

// Creates a custom action promo that opens the specified side panel.
std::optional<FeaturePromoSpecification> CreateSidePanelPromo(
    int body_text_id,
    int action_text_id,
    SidePanelEntryId side_panel_id,
    bool google_is_default_search_provider) {
  if (side_panel_id == SidePanelEntryId::kCustomizeChrome &&
      !google_is_default_search_provider) {
    return std::nullopt;
  }

  auto spec = FeaturePromoSpecification::CreateForCustomAction(
      kLowUsagePromo, kTopContainerElementId, body_text_id, action_text_id,
      base::BindRepeating(
          [](SidePanelEntryId side_panel_id, ui::ElementContext ctx,
             FeaturePromoHandle) {
            if (Browser* const browser = ContextToBrowser(ctx)) {
              switch (side_panel_id) {
                case SidePanelEntryId::kCustomizeChrome:
                  // For customization, the NTP must be shown first.
                  if (auto* const contents = NavigateToPage(
                          browser, GURL(chrome::kChromeUINewTabURL))) {
                    OpenSidePanelOnNavigation::Show(contents, side_panel_id);
                  }
                  break;
                default:
                  ShowSidePanel(browser, side_panel_id);
                  break;
              }
            }
          },
          side_panel_id));
  spec.SetBubbleArrow(HelpBubbleArrow::kNone);
  return spec;
}

// Creates a custom action promo that just displays a message.
FeaturePromoSpecification CreateMessageOnlyPromo(int body_text_id,
                                                 int screenreader_text_id = 0) {
  auto spec = FeaturePromoSpecification::CreateForToastPromo(
      kLowUsagePromo, kTopContainerElementId, body_text_id,
      screenreader_text_id, AcceleratorInfo());
  spec.SetBubbleArrow(HelpBubbleArrow::kNone);
  return spec;
}

}  // namespace

FeaturePromoSpecification CreateLowUsagePromoSpecification(Profile* profile) {
  const bool show_ai_promos = base::GetFieldTrialParamByFeatureAsBool(
      kLowUsagePromo, kIncludeAiPromosFieldTrialParam, false);

  const bool google_is_default_search_provider =
      profile && search::DefaultSearchProviderIsGoogle(profile);

  // Note: numbers correspond to indices in the spec; they are preserved here to
  // ensure that if promos need to be changed or re-ordered we can retain the
  // correspondence.
  auto spec = FeaturePromoSpecification::CreateForRotatingPromo(
      kLowUsagePromo,

      // 1.
      CreateSidePanelPromo(IDS_REENGAGEMENT_PROMO_PERSONALIZE,
                           IDS_REENGAGEMENT_PROMO_CUSTOMIZE_CHROME_ACTION,
                           SidePanelEntryId::kCustomizeChrome,
                           google_is_default_search_provider),

      // 8.
      show_ai_promos
          ? CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_EXPLORE_AI,
                                IDS_REENGAGEMENT_PROMO_EXPLORE_AI_ACTION,
                                GURL(kGoogleChromeAiUrl))
          : CreateMessageOnlyPromo(IDS_REENGAGEMENT_PROMO_AI_BENEFITS),

      // 4.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_PASSWORD_MANAGER,
                          IDS_REENGAGEMENT_PROMO_LEARN_HOW_ACTION,
                          GURL(kGooglePasswordsUrl)),

      // 3.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_EXTENSIONS,
                          IDS_REENGAGEMENT_PROMO_VISIT_CHROME_WEB_STORE_ACTION,
                          GURL(kExtensionsWebStoreUrl)),

      // 15.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_PERFORMANCE,
                          IDS_REENGAGEMENT_PROMO_SETTINGS_ACTION,
                          chrome::GetSettingsUrl(chrome::kPerformanceSubPage)),

      // 5.
      show_ai_promos
          ? CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_AI_WRITING,
                                IDS_REENGAGEMENT_PROMO_LEARN_HOW_ACTION,
                                GURL(kGoogleChromeAiUrl))
          : CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_TRANSLATE,
                                IDS_REENGAGEMENT_PROMO_LEARN_HOW_ACTION,
                                GURL(kBrowserToolsUrl)),

#if !BUILDFLAG(IS_CHROMEOS)
      // 7.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_NEW_FEATURES,
                          IDS_REENGAGEMENT_PROMO_VIEW_WHATS_NEW_ACTION,
                          GURL(chrome::kChromeUIWhatsNewURL)),
#endif

      // 9.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_SAFETY,
                          IDS_REENGAGEMENT_PROMO_VIEW_SAFETY_FEATURES_ACTION,
                          GURL(kGoogleChromeSafetyUrl)),

      // 2.
      CreateSidePanelPromo(IDS_REENGAGEMENT_PROMO_CUSTOMIZE_COLOR,
                           IDS_REENGAGEMENT_PROMO_CUSTOMIZE_CHROME_ACTION,
                           SidePanelEntryId::kCustomizeChrome,
                           google_is_default_search_provider),

#if !BUILDFLAG(IS_CHROMEOS)
      // 10.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_LATEST_TECHNOLOGY,
                          IDS_REENGAGEMENT_PROMO_VIEW_WHATS_NEW_ACTION,
                          GURL(chrome::kChromeUIWhatsNewURL)),
#endif

      // 6.
      show_ai_promos
          ? CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_AI_THEME,
                                IDS_REENGAGEMENT_PROMO_LEARN_HOW_ACTION,
                                GURL(kGoogleChromeAiUrl))
          : CreateNavigatePromo(
                IDS_REENGAGEMENT_PROMO_THEMES_WEB_STORE,
                IDS_REENGAGEMENT_PROMO_VISIT_CHROME_WEB_STORE_ACTION,
                GURL(kThemesWebStoreUrl)),

      // 17.
      CreateMessageOnlyPromo(IDS_REENGAGEMENT_PROMO_CROSS_PLATFORM_FAMILIAR),

      // 13.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_ADDRESS_BAR,
                          IDS_REENGAGEMENT_PROMO_LEARN_HOW_ACTION,
                          GURL(kBrowserFeaturesUrl)),

      // 11.
      show_ai_promos
          ? CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_AI_ORGANIZE_TABS,
                                IDS_REENGAGEMENT_PROMO_LEARN_HOW_ACTION,
                                GURL(kGoogleChromeAiUrl))
          : CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_TAB_GROUPS,
                                IDS_REENGAGEMENT_PROMO_LEARN_HOW_ACTION,
                                GURL(kBrowserFeaturesUrl)),

      // 12.
      CreateNavigatePromo(
          IDS_REENGAGEMENT_PROMO_DEFAULT_BROWSER,
          IDS_REENGAGEMENT_PROMO_SETTINGS_ACTION,
          chrome::GetSettingsUrl(chrome::kDefaultBrowserSubPage)),

      // 20.
      CreateMessageOnlyPromo(IDS_REENGAGEMENT_PROMO_CROSS_PLATFORM_SECURE),

      // 18.
      CreateMessageOnlyPromo(IDS_REENGAGEMENT_PROMO_TAB_PIN_AND_GROUP),

      // 19.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_SAFE_BROWSING,
                          IDS_REENGAGEMENT_PROMO_LEARN_HOW_ACTION,
                          GURL(kChromeSecurityBlogUrl)),

      // 14.
      CreateMessageOnlyPromo(IDS_REENGAGEMENT_PROMO_SIGN_IN),

      // 16.
      CreateNavigatePromo(IDS_REENGAGEMENT_PROMO_ENERGY_MEMORY_SAVER,
                          IDS_REENGAGEMENT_PROMO_SETTINGS_ACTION,
                          chrome::GetSettingsUrl(chrome::kPerformanceSubPage)));

  // Allow focus based on field trial param; default is no focus.
  spec.OverrideFocusOnShow(base::GetFieldTrialParamByFeatureAsBool(
      kLowUsagePromo, kLowUsagePromoFocusFieldTrialParam, false));

  return spec;
}
