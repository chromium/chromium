// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_queue_manager.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/feature_first_run/autofill_ai_first_run_dialog.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/most_recent_shared_tab_update_store.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_view_iph_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/user_education/autofill_help_bubble_factory.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/views/user_education/browser_ntp_promos.h"
#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_20.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_controller_25.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/browser/ui/views/user_education/ios_promo_bubble_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/user_education/ntp_promo_identifiers.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/data_sharing/public/features.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "components/plus_addresses/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_handle.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/new_badge/new_badge_specification.h"
#include "components/user_education/common/tutorial/tutorial_description.h"
#include "components/user_education/common/tutorial/tutorial_registry.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_metadata.h"
#include "components/user_education/views/custom_help_bubble_view.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/extension_urls.h"
#include "pdf/buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/plus_addresses/core/browser/resources/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/user_education/views/help_bubble_factory_views_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "components/user_education/views/help_bubble_factory_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_ui.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#endif

#if BUILDFLAG(ENABLE_PDF_INK2)
#include "chrome/browser/pdf/pdf_help_bubble_handler_factory.h"
#endif

namespace {

using Platforms = user_education::Metadata::Platforms;
constexpr std::initializer_list<Platforms> kComposePlatforms{
    Platforms::kWindows, Platforms::kMac, Platforms::kLinux};

constexpr char kTabGroupHeaderElementName[] = "TabGroupHeader";
constexpr char kChromeThemeBackElementName[] = "ChromeThemeBackElement";
constexpr char kLastInactiveTabElementName[] = "LastInactiveTab";

class IfView : public user_education::TutorialDescription::If {
 public:
  template <typename V>
  IfView(user_education::TutorialDescription::ElementSpecifier element,
         base::RepeatingCallback<bool(const V*)> if_condition)
      : If(element,
           base::BindRepeating(
               [](base::RepeatingCallback<bool(const V*)> if_condition,
                  const ui::TrackedElement* el) {
                 return if_condition.Run(views::AsViewClass<V>(
                     el->AsA<views::TrackedElementViews>()->view()));
               },
               std::move(if_condition))) {}
};

bool HasTabGroups(const BrowserView* browser_view) {
  return !browser_view->browser()
              ->tab_strip_model()
              ->group_model()
              ->ListTabGroups()
              .empty();
}

using ContextPtr = const user_education::UserEducationContextPtr&;

// Convenience method to get the browser view from the context.
BrowserView& GetBrowserView(ContextPtr context) {
  auto* const browser_context = context->AsA<BrowserUserEducationContext>();
  CHECK(browser_context);
  return browser_context->GetBrowserView();
}

// Convenience method to get the browser from the context.
Browser* GetBrowser(ContextPtr context) {
  return GetBrowserView(context).browser();
}

// Returns a `CustomActionCallback` that navigates to `target` in a new tab.
user_education::FeaturePromoSpecification::CustomActionCallback
CreateNavigationAction(GURL target) {
  return base::BindRepeating(
      [](GURL url, ContextPtr ctx,
         user_education::FeaturePromoHandle promo_handle) {
        auto* browser = GetBrowser(ctx);
        NavigateParams params(browser->profile(), url,
                              ui::PAGE_TRANSITION_LINK);
        params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        params.browser = browser;
        Navigate(&params);
      },
      std::move(target));
}

}  // namespace

user_education::HelpBubbleDelegate* GetHelpBubbleDelegate() {
  static base::NoDestructor<BrowserHelpBubbleDelegate> delegate;
  return delegate.get();
}

void RegisterChromeHelpBubbleFactories(
    user_education::HelpBubbleFactoryRegistry& registry) {
  const user_education::HelpBubbleDelegate* const delegate =
      GetHelpBubbleDelegate();
#if BUILDFLAG(IS_CHROMEOS)
  // Try to create an Ash-specific help bubble first. Note that an Ash-specific
  // help bubble will only take precedence over a standard Views-specific help
  // bubble if the tracked element's help bubble context is explicitly set to
  // `ash::HelpBubbleContext::kAsh`.
  registry.MaybeRegister<ash::HelpBubbleFactoryViewsAsh>(delegate);
#endif  // BUILDFLAG(IS_CHROMEOS)
  // Autofill bubbles require special handling.
  registry.MaybeRegister<AutofillHelpBubbleFactory>(delegate);
  registry.MaybeRegister<user_education::HelpBubbleFactoryViews>(delegate);
  // Try to create a floating bubble first, if it's allowed.
  registry.MaybeRegister<FloatingWebUIHelpBubbleFactoryBrowser>(delegate);
  // Fall back to in-WebUI help bubble if the floating bubble doesn't apply.
  registry.MaybeRegister<TabWebUIHelpBubbleFactoryBrowser>();
#if BUILDFLAG(IS_MAC)
  registry.MaybeRegister<user_education::HelpBubbleFactoryMac>(delegate);
#endif
}

void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry,
    Profile* profile) {
  using user_education::FeaturePromoSpecification;
  using user_education::HelpBubbleArrow;
  using user_education::Metadata;
  using AdditionalCondition =
      FeaturePromoSpecification::AdditionalConditions::AdditionalCondition;
  using AdditionalConditions = FeaturePromoSpecification::AdditionalConditions;

  // This icon got updated, so select the 2023 Refresh version.
  // Note that the WebUI refresh state is not taken into account, so
  // this selection will affect both Views and WebUI help bubbles.
  const gfx::VectorIcon* const kLightbulbOutlineIcon =
      &vector_icons::kLightbulbOutlineChromeRefreshIcon;

  // Verify that we haven't already registered the expected features.
  // Use a known test feature that is unlikely to change.
  if (registry.IsFeatureRegistered(
          feature_engagement::kIPHWebUiHelpBubbleTestFeature)) {
    return;
  }

  // kIPHAutofillCreditCardBenefitFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillCreditCardBenefitFeature,
          autofill::PopupViewViews::kAutofillCreditCardBenefitElementId,
          IDS_AUTOFILL_CREDIT_CARD_BENEFIT_IPH_BUBBLE_LABEL,
          IDS_AUTOFILL_CREDIT_CARD_BENEFIT_IPH_BUBBLE_LABEL_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(125, "alexandertekle@google.com",
                       "Triggered after a credit card benefit is displayed for "
                       "the first time.")));

  // TODO(crbug.com/40264177): Use toast or snooze instead of legacy promo.
  // kIPHAutofillExternalAccountProfileSuggestionFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::
                        kIPHAutofillExternalAccountProfileSuggestionFeature,
                    autofill::PopupViewViews::kAutofillSuggestionElementId,
                    IDS_AUTOFILL_IPH_EXTERNAL_ACCOUNT_PROFILE_SUGGESTION)
                    .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
                    .SetMetadata(115, "vykochko@chromium.org",
                                 "Triggered after autofill popup appears.")));

  // TODO(crbug.com/397940269): Check if
  // `IDS_AUTOFILL_IPH_HOME_AND_WORK_ACCOUNT_PROFILE_SUGGESTION_SCREENREADER`
  // should be same as
  // `IDS_AUTOFILL_IPH_HOME_AND_WORK_ACCOUNT_PROFILE_SUGGESTION` once the
  // strings are finalized. kIPHAutofillHomeWorkProfileSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillHomeWorkProfileSuggestionFeature,
          autofill::PopupViewViews::kAutofillHomeWorkSuggestionElementId,
          IDS_AUTOFILL_IPH_HOME_AND_WORK_ACCOUNT_PROFILE_SUGGESTION,
          IDS_AUTOFILL_IPH_HOME_AND_WORK_ACCOUNT_PROFILE_SUGGESTION_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetBubbleTitleText(
              IDS_AUTOFILL_IPH_HOME_AND_WORK_ACCOUNT_PROFILE_SUGGESTION_TITLE)
          .SetMetadata(136, "vidhanj@google.com",
                       "Triggered after a home/work suggestion is available to "
                       "user for filling")));

  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillAccountNameEmailSuggestionFeature,
          autofill::PopupViewViews::
              kAutofillAccountNameEmailSuggestionElementId,
          IDS_AUTOFILL_IPH_ACCOUNT_NAME_EMAIL_SUGGESTION,
          IDS_AUTOFILL_IPH_ACCOUNT_NAME_EMAIL_SUGGESTION_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetBubbleTitleText(
              IDS_AUTOFILL_IPH_ACCOUNT_NAME_EMAIL_SUGGESTION_TITLE)
          .SetMetadata(
              143, "sygiet@google.com",
              "Triggered after a name and email suggestion is available to "
              "user for filling")));

  // kIPHAutofillAiOptInFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHAutofillAiOptInFeature,
          autofill::PopupViewViews::kAutofillAiOptInIphElementId,
          IDS_AUTOFILL_AI_OPT_IN_IPH_BODY, IDS_AUTOFILL_AI_OPT_IN_IPH_TURN_ON,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                Browser* const browser = GetBrowser(ctx);
                TabStripModel* const tab_strip_model =
                    browser->tab_strip_model();
                if (!tab_strip_model) {
                  return;
                }
                content::WebContents* const web_contents =
                    tab_strip_model->GetActiveWebContents();
                feature_first_run::ShowAutofillAiFirstRunDialog(web_contents);
              }))
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_AUTOFILL_AI_OPT_IN_IPH_MAYBE_LATER)
          .SetBubbleTitleText(IDS_AUTOFILL_AI_OPT_IN_IPH_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .AddPreconditionExemption(kUserNotActivePrecondition)
          .SetMetadata(136, "brunobraga@google.com",
                       "Displayed on input fields that are eligible for "
                       "AutofillAI. These can be input fields on any website "
                       "as long as the field has AutofillAI predictions. "
                       "The IPH is displayed when the user clicks on such an "
                       "input field and is anchored against it.")));

  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillAiValuablesFeature,
          autofill::PopupViewViews::kAutofillAiValuablesElementId,
          IDS_AUTOFILL_AI_VALUABLES_IPH_BODY,
          IDS_AUTOFILL_IPH_LOYALTY_CARD_SUGGESTION_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleTitleText(IDS_AUTOFILL_AI_VALUABLES_IPH_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(
              143, "vizcay@google.com",
              "Triggered after Wallet autofill suggestions are shown.")));

  // kIPHAutofillVirtualCardCVCSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature,
          autofill::PopupViewViews::kAutofillStandaloneCvcSuggestionElementId,
          IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_IPH_BUBBLE_LABEL,
          IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_IPH_BUBBLE_LABEL_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(118, "alexandertekle@google.com",
                       "Triggered after autofill popup appears.")));

  // kIPHAutofillVirtualCardSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature,
          autofill::PopupViewViews::kAutofillCreditCardSuggestionEntryElementId,
          IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL)
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(100, "siyua@chromium.org",
                       "Triggered after autofill popup appears.")));

  bool bnpl_second_line_string_experiment_enabled =
      base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString);

  // kIPHAutofillBnplAffirmOrZipSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillBnplAffirmOrZipSuggestionFeature,
          autofill::PopupViewViews::kAutofillBnplAffirmOrZipSuggestionElementId,
          bnpl_second_line_string_experiment_enabled
              ? IDS_AUTOFILL_CARD_BNPL_SUGGESTION_WITH_GOOGLE_PAY_IPH_BUBBLE_LABEL_DESKTOP
              : IDS_AUTOFILL_CARD_BNPL_AFFIRM_OR_ZIP_SUGGESTION_IPH_BUBBLE_LABEL_DESKTOP,
          bnpl_second_line_string_experiment_enabled
              ? IDS_AUTOFILL_CARD_BNPL_SUGGESTION_WITH_GOOGLE_PAY_IPH_BUBBLE_LABEL_DESKTOP_SCREENREADER
              : IDS_AUTOFILL_CARD_BNPL_AFFIRM_OR_ZIP_SUGGESTION_IPH_BUBBLE_LABEL_DESKTOP_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(137, "yiwenqian@google.com",
                       "Triggered when users see the BNPL chip. Used when the "
                       "possible available BNPL issuers are Affirm and Zip.")));

  // kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::
              kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature,
          autofill::PopupViewViews::
              kAutofillBnplAffirmZipOrKlarnaSuggestionElementId,
          bnpl_second_line_string_experiment_enabled
              ? IDS_AUTOFILL_CARD_BNPL_SUGGESTION_WITH_GOOGLE_PAY_IPH_BUBBLE_LABEL_DESKTOP
              : IDS_AUTOFILL_CARD_BNPL_AFFIRM_ZIP_OR_KLARNA_SUGGESTION_IPH_BUBBLE_LABEL_DESKTOP,
          bnpl_second_line_string_experiment_enabled
              ? IDS_AUTOFILL_CARD_BNPL_SUGGESTION_WITH_GOOGLE_PAY_IPH_BUBBLE_LABEL_DESKTOP_SCREENREADER
              : IDS_AUTOFILL_CARD_BNPL_AFFIRM_ZIP_OR_KLARNA_SUGGESTION_IPH_BUBBLE_LABEL_DESKTOP_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(
              139, "wilsonlow@google.com",
              "Triggered when users see the BNPL chip. Used when the possible "
              "available BNPL issuers are Affirm, Zip, and Klarna.")));

  // kIPHAutofillCardInfoRetrievalSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillCardInfoRetrievalSuggestionFeature,
          autofill::PopupViewViews::kAutofillCreditCardSuggestionEntryElementId,
          IDS_AUTOFILL_CARD_INFO_RETRIEVAL_SUGGESTION_IPH_BUBBLE_LABEL,
          IDS_AUTOFILL_CARD_INFO_RETRIEVAL_SUGGESTION_IPH_BUBBLE_LABEL_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(135, "jialihuang@google.com",
                       "Triggered after autofill popup appears for a card "
                       "enrolled in card info retrieval.")));

  // kIPHAutofillDisabledVirtualCardSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillDisabledVirtualCardSuggestionFeature,
          autofill::PopupViewViews::kAutofillCreditCardSuggestionEntryElementId,
          IDS_AUTOFILL_DISABLED_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL_DESKTOP,
          IDS_AUTOFILL_DISABLED_VIRTUAL_CARD_SUGGESTION_IPH_BUBBLE_LABEL_DESKTOP_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(130, "hvs@google.com",
                       "Triggered after autofill popup appears for disabled "
                       "virtual card.")));

  // kIPHCreatePlusAddressSuggestionFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHPlusAddressCreateSuggestionFeature,
          kPlusAddressCreateSuggestionElementId,
          IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH,
          IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(128, "vidhanj@google.com",
                       "Triggered after create plus address popup appears.")));

  // kIPHPlusAddressFirstSaveFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHPlusAddressFirstSaveFeature,
          kToolbarAvatarButtonElementId,
          IDS_PLUS_ADDRESS_FIRST_SAVE_IPH_DESCRIPTION,
          IDS_PLUS_ADDRESS_FIRST_SAVE_IPH_ACCEPT,
          CreateNavigationAction(
              GURL(plus_addresses::features::kPlusAddressManagementUrl.Get())))
          .SetCustomActionIsDefault(true)
          .SetBubbleIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
              &plus_addresses::kPlusAddressLogoSmallIcon
#else
              &vector_icons::kEmailIcon
#endif
              )
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleTitleText(IDS_PLUS_ADDRESS_FIRST_SAVE_IPH_TITLE)
          .SetMetadata(
              131, "jkeitel@google.com",
              "Triggered after first creation of a plus address on Desktop.")));

  // TODO(crbug.com/404437008): Update with final IPH strings.
  // kIPHAutofillEnableLoyaltyCardsFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHAutofillEnableLoyaltyCardsFeature,
          autofill::PopupViewViews::kAutofillEnableLoyaltyCardsElementId,
          IDS_AUTOFILL_IPH_LOYALTY_CARD_SUGGESTION_BODY,
          IDS_AUTOFILL_IPH_LOYALTY_CARD_SUGGESTION_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleTitleText(IDS_AUTOFILL_IPH_LOYALTY_CARD_SUGGESTION_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
          .SetMetadata(
              137, "vizcay@google.com",
              "Triggered after loyalty card autofill suggestions are shown.")));

  // kIPHDesktopPwaInstallFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHDesktopPwaInstallFeature,
                    kInstallPwaElementId, IDS_DESKTOP_PWA_INSTALL_PROMO)
                    .SetMetadata(89, "phillis@chromium.org",
                                 "Triggered after user navigates to a "
                                 "page with a promotable PWA.")));

  // kIPHDesktopCustomizeChromeExperimentFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDesktopCustomizeChromeExperimentFeature,
          kTopContainerElementId,
          IDS_TUTORIAL_CUSTOMIZE_CHROME_START_TUTORIAL_IPH,
          IDS_PROMO_SHOW_TUTORIAL_BUTTON,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                Browser* const browser = GetBrowser(ctx);
                if (!search::DefaultSearchProviderIsGoogle(
                        browser->profile())) {
                  return;
                }
                auto* service =
                    UserEducationServiceFactory::GetForBrowserContext(
                        browser->profile());
                user_education::TutorialService* tutorial_service =
                    service ? &service->tutorial_service() : nullptr;
                if (!tutorial_service) {
                  return;
                }
                TabStripModel* tab_strip_model = browser->tab_strip_model();
                if (tab_strip_model) {
                  content::WebContents* web_contents =
                      tab_strip_model->GetActiveWebContents();
                  if (web_contents &&
                      web_contents->GetURL() != browser->GetNewTabURL()) {
                    NavigateParams params(browser->profile(),
                                          GURL(chrome::kChromeUINewTabPageURL),
                                          ui::PAGE_TRANSITION_LINK);
                    params.disposition =
                        WindowOpenDisposition::NEW_FOREGROUND_TAB;
                    Navigate(&params);
                  }
                }
                user_education::TutorialIdentifier tutorial_id =
                    kSidePanelCustomizeChromeTutorialId;

                tutorial_service->StartTutorial(tutorial_id,
                                                ctx->GetElementContext());
                tutorial_service->LogIPHLinkClicked(tutorial_id, true);
              }))
          .SetBubbleArrow(HelpBubbleArrow::kNone)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_PROMO_SNOOZE_BUTTON)
          // See: crbug.com/1494923
          .OverrideFocusOnShow(false)
          .SetMetadata(143, "rsult@google.com",
                       "Intro for the Customize Chrome Tutorial. Triggered "
                       "upon Chrome Startup for users who have not customized "
                       "Chrome yet or have not interacted with the 'Customize "
                       "Chrome' button.")));

  // kIPHDesktopCustomizeChromeAutoPromoFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHDesktopCustomizeChromeAutoOpenFeature,
          CustomizeButtonsHandler::kCustomizeChromeButtonElementId,
          IDS_IPH_CUSTOMIZE_CHROME_AUTO_OPEN_BODY,
          IDS_IPH_CUSTOMIZE_CHROME_AUTO_OPEN_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
          .SetInAnyContext(true)
          .SetMetadata(143, "kbajkiewicz@google.com",
                       "Attempts to trigger when user is on NTP to promote "
                       "customization.")));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // kIPHExtensionsMenuFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHExtensionsMenuFeature,
          kExtensionsMenuButtonElementId,
          IDS_EXTENSIONS_MENU_IPH_ENTRY_POINT_BODY)
          .SetBubbleTitleText(IDS_EXTENSIONS_MENU_IPH_ENTRY_POINT_TITLE)
          .SetMetadata(
              117, "emiliapaz@chromium.org",
              "Triggered when an extension already has access permission.")));

  // kIPHExtensionsRequestAccessButtonFeature
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHExtensionsRequestAccessButtonFeature,
          kExtensionsRequestAccessButtonElementId,
          IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_IPH_ENTRY_POINT_BODY)
          .SetBubbleTitleText(
              IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_IPH_ENTRY_POINT_TITLE)
          .SetMetadata(117, "emiliapaz@chromium.org",
                       "Triggered when an extension "
                       "requests access permission.")));

  // kIPHExtensionsZeroStatePromoFeature
  Metadata iph_extensions_zero_state_promo_feature_metaData(
      140, "uwyiming@google.com",
      "Triggered when a user has no "
      "extensions installed.");
  switch (feature_engagement::kIPHExtensionsZeroStatePromoVariantParam.Get()) {
    case feature_engagement::kCustomActionIph:
      registry.RegisterFeature(std::move(
          FeaturePromoSpecification::CreateForCustomAction(
              feature_engagement::kIPHExtensionsZeroStatePromoFeature,
              kToolbarAppMenuButtonElementId,
              IDS_EXTENSIONS_ZERO_STATE_PROMO_CUSTOM_ACTION_IPH_DESCRIPTION,
              IDS_EXTENSIONS_ZERO_STATE_PROMO_CUSTOM_ACTION_IPH_ACCEPT,
              CreateNavigationAction(extension_urls::AppendUtmSource(
                  extension_urls::GetNewWebstoreLaunchURL(),
                  extension_urls::kCustomActionIphUtmSource)))
              .SetCustomActionIsDefault(true)
              .SetBubbleTitleText(IDS_EXTENSIONS_ZERO_STATE_PROMO_IPH_TITLE)
              .SetMetadata(
                  std::move(iph_extensions_zero_state_promo_feature_metaData))
              .SetHighlightedMenuItem(
                  ExtensionsMenuModel::kVisitChromeWebStoreMenuItem)));
      break;
    case feature_engagement::kCustomUiChipIphV1:
    case feature_engagement::kCustomUiChipIphV2:
    case feature_engagement::kCustomUiChipIphV3:
    case feature_engagement::kCustomUIPlainLinkIph:
      registry.RegisterFeature(std::move(
          FeaturePromoSpecification::CreateForCustomUi(
              feature_engagement::kIPHExtensionsZeroStatePromoFeature,
              kToolbarAppMenuButtonElementId,
              MakeCustomWebUIHelpBubbleFactoryCallback<
                  extensions::ZeroStatePromoController>(
                  GURL(chrome::kChromeUIExtensionsZeroStatePromoURL)),
              // No op. The individual buttons on the custom UI will perform the
              // actual actions.
              base::DoNothing())
              .SetMetadata(
                  std::move(iph_extensions_zero_state_promo_feature_metaData))
              .SetBubbleArrow(HelpBubbleArrow::kTopRight)
              .SetHighlightedMenuItem(
                  ExtensionsMenuModel::kVisitChromeWebStoreMenuItem)));
      break;
  }
#endif

  // kIPHLiveCaptionFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHLiveCaptionFeature, kToolbarMediaButtonElementId,
      IDS_LIVE_CAPTION_PROMO, IDS_LIVE_CAPTION_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHTabAudioMutingFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHTabAudioMutingFeature,
          kTabAlertIndicatorButtonElementId, IDS_TAB_AUDIO_MUTING_PROMO,
          IDS_LIVE_CAPTION_PROMO_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopCenter)));

#if BUILDFLAG(ENABLE_GLIC)
  // kIPHGlicPromoFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHGlicPromoFeature, kGlicButtonElementId,
          IDS_GLIC_PROMO_BODY)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleTitleText(IDS_GLIC_PROMO_TITLE)
          // Since this can appear randomly, we do not want to steal focus from
          // the user; see https://crbug.com/418579754
          .OverrideFocusOnShow(false)
          .SetMetadata(
              133, "dfried@chromium.org",
              "Attempts to trigger when the user is on a supported page.")));

  // kIPHGlicTryItFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHGlicTryItFeature, kGlicButtonElementId,
          IDS_GLIC_TRYIT_BODY, IDS_GLIC_PROMO_CONFIRM,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = GetBrowser(ctx);
                if (!browser) {
                  return;
                }
                if (auto* glic_service =
                        glic::GlicKeyedService::Get(browser->GetProfile())) {
                  glic_service->ToggleUI(
                      browser, /*prevent_close=*/true,
                      glic::mojom::InvocationSource::kTopChromeButton);
                }
              }))
          .SetBubbleTitleText(IDS_GLIC_TRYIT_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetCustomActionIsDefault(true)
          // Since this can appear randomly, we do not want to steal focus from
          // the user; see https://crbug.com/418579754
          .OverrideFocusOnShow(false)
          .SetMetadata(
              142, "dewittj@chromium.org",
              "Attempts to trigger when the user is on a supported page.")));

  // kGlicTrustFirstOnboarding shortcut toast IPH:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::
              kIPHGlicTrustFirstOnboardingShortcutToastPromoFeature,
          kGlicButtonElementId, IDS_GLIC_SHORTCUT_IPH_TEXT_TEMPLATE,
          IDS_GLIC_SHORTCUT_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo(
              glic::LocalHotkeyManager::GetConfigurableAccelerator(
                  glic::LocalHotkeyManager::Hotkey::kFocusToggle)))
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetMetadata(144, "zalmashni@google.com",
                       "Triggered after the Glic side panel is closed or the "
                       "user navigates to a new tab.")));

  // kGlicTrustFirstOnboarding shortcut snooze IPH:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::
              kIPHGlicTrustFirstOnboardingShortcutSnoozePromoFeature,
          kGlicButtonElementId, IDS_GLIC_SHORTCUT_IPH_TEXT_TEMPLATE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetMetadata(144, "zalmashni@google.com",
                       "Triggered after the Glic side panel is closed or the "
                       "user navigates to a new tab.")));
#endif  // BUILDFLAG(ENABLE_GLIC)

  // kIPHGMCCastStartStopFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForLegacyPromo(
      &feature_engagement::kIPHGMCCastStartStopFeature,
      kToolbarMediaButtonElementId,
      IDS_GLOBAL_MEDIA_CONTROLS_CONTROL_CAST_SESSIONS_PROMO));

  // kIPHGMCLocalMediaCastingFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHGMCLocalMediaCastingFeature,
      kToolbarMediaButtonElementId, IDS_GMC_LOCAL_MEDIA_CAST_SESSIONS_PROMO,
      IDS_GMC_LOCAL_MEDIA_CAST_START_PROMO,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHPasswordsSavePrimingPromo:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHPasswordsSavePrimingPromoFeature,
#if BUILDFLAG(IS_CHROMEOS)
          // No avatar button on ChromeOS, so anchor to app menu instead.
          kToolbarAppMenuButtonElementId,
#else
          kToolbarAvatarButtonElementId,
#endif
          IDS_PASSWORDS_SAVE_PRIMING_PROMO_BODY_TEMPLATE,
          IDS_PASSWORDS_SAVE_PRIMING_PROMO_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetMetadata(
              137, "dfried@chromium.org",
              "Triggered when the user navigates a page with an eligible login "
              "form, and they have no saved passwords.")));

  // kIPHPasswordsSavePrimingPromo:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHPasswordsSaveRecoveryPromoFeature,
                    kPasswordsOmniboxKeyIconElementId,
                    IDS_PASSWORDS_SAVE_RECOVERY_PROMO_BODY,
                    IDS_PASSWORDS_SAVE_RECOVERY_PROMO_SCREENREADER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetMetadata(137, "dfried@chromium.org",
                                 "Triggered when the user logs into a page "
                                 "they have blocklisted")));

  // kIPHPasswordsManagementBubbleAfterSaveFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHPasswordsManagementBubbleAfterSaveFeature,
          kPasswordsOmniboxKeyIconElementId,
          IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_AFTER_SAVE,
          IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_AFTER_SAVE_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetMetadata(113, "mamir@chromium.org",
                       "Triggered once when user has saved a password.")));

  // kIPHPasswordsManagementBubbleDuringSigninFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature,
      kPasswordsOmniboxKeyIconElementId,
      IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_DURING_SIGNIN,
      IDS_PASSWORD_MANAGER_IPH_MANAGEMENT_BUBBLE_DURING_SIGNIN_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHPasswordManagerShortcutFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForTutorialPromo(
          feature_engagement::kIPHPasswordManagerShortcutFeature,
          kPasswordsOmniboxKeyIconElementId,
          IDS_PASSWORD_MANAGER_IPH_CREATE_SHORTCUT_BODY,
          kPasswordManagerTutorialId)
          .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetBubbleTitleText(IDS_PASSWORD_MANAGER_IPH_CREATE_SHORTCUT_TITLE)));

#if BUILDFLAG(ENABLE_PDF_INK2)
  // kIPHPdfInkSignaturesFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHPdfInkSignaturesFeature,
          pdf::PdfHelpBubbleHandlerFactory::kPdfInkSignaturesDrawElementId,
          IDS_PDF_INK_SIGNATURES_IPH_BODY)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetInAnyContext(true)
          .SetMetadata(138, "thestig@chromium.org",
                       "Triggered when the PDF Viewer opens.")));
#endif

  // kIPHPdfSearchifyFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHPdfSearchifyFeature, kTopContainerElementId,
          IDS_PDF_SEARCHIFY_IPH_BODY, IDS_PDF_SEARCHIFY_IPH_BODY_SCREEN_READER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kNone)
          .SetBubbleTitleText(IDS_PDF_SEARCHIFY_IPH_TITLE)
          .SetMetadata(132, "rhalavati@chromium.org",
                       "Triggered once when user opens a PDF which gets OCRed.")
          .SetAnchorElementFilter(base::BindRepeating(
              [](const ui::ElementTracker::ElementList& elements)
                  -> ui::TrackedElement* {
                if (elements.empty()) {
                  return nullptr;
                }
                // Ensure a searchified PDF is visible before showing the IPH.
                auto* const browser_view =
                    views::ElementTrackerViews::GetInstance()
                        ->GetFirstMatchingViewAs<BrowserView>(
                            kBrowserViewElementId, elements[0]->context());
                std::vector<ContentsWebView*> contents_web_views =
                    browser_view->GetAllVisibleContentsWebViews();
                for (auto* contents_web_view : contents_web_views) {
                  auto* pdf_doc_helper =
                      pdf::PDFDocumentHelper::MaybeGetForWebContents(
                          contents_web_view->GetWebContents());
                  if (pdf_doc_helper && pdf_doc_helper->SearchifyStarted()) {
                    return elements[0];
                  }
                }
                return nullptr;
              }))));

  // kIPHLensOverlayFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForTutorialPromo(
          feature_engagement::kIPHLensOverlayFeature,
          kToolbarAppMenuButtonElementId,
          IDS_TUTORIAL_LENS_OVERLAY_HOMEWORK_INTRO_BODY, kLensOverlayTutorialId)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetBubbleTitleText(IDS_TUTORIAL_LENS_OVERLAY_HOMEWORK_INTRO_HEADER)
          .SetMetadata(131, "nguyenbryan@google.com",
                       "Triggered by certain URLs to start the Lens Overlay "
                       "tutorial.")));

  // kIPHPasswordSharingFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHPasswordSharingFeature,
                    PasswordManagerUI::kSharePasswordElementId,
                    IDS_PASSWORD_MANAGER_IPH_SHARE_PASSWORD_BUTTON,
                    IDS_PASSWORD_MANAGER_IPH_SHARE_PASSWORD_BUTTON_SCREENREADER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetInAnyContext(true)
                    .SetBubbleIcon(kLightbulbOutlineIcon)
                    .SetBubbleArrow(HelpBubbleArrow::kTopRight)));

  // kIPHPowerBookmarksSidePanelFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                    feature_engagement::kIPHPowerBookmarksSidePanelFeature,
                    kToolbarAppMenuButtonElementId,
                    IDS_POWER_BOOKMARKS_SIDE_PANEL_PROMO_PINNING)
                    .SetHighlightedMenuItem(
                        BookmarkSubMenuModel::kShowBookmarkSidePanelItem)
                    .SetMetadata(121, "emshack@chromium.org",
                                 "Triggered when a bookmark is added from the "
                                 "bookmark page action in omnibox.")));

#if !BUILDFLAG(IS_CHROMEOS)
  // kIPHSwitchProfileFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHProfileSwitchFeature,
      kToolbarAvatarButtonElementId, IDS_PROFILE_SWITCH_PROMO,
      IDS_PROFILE_SWITCH_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo(IDC_SHOW_AVATAR_MENU)));

  // kIPHPasswordsWebAppProfileSwitchFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForToastPromo(
      feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature,
      kToolbarAvatarButtonElementId,
      IDS_PASSWORD_MANAGER_IPH_BODY_WEB_APP_PROFILE_SWITCH,
      IDS_PROFILE_SWITCH_PROMO_SCREENREADER,
      FeaturePromoSpecification::AcceleratorInfo()));

  // kIPHExplicitBrowserSigninPreferenceRememberedFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::
              kIPHExplicitBrowserSigninPreferenceRememberedFeature,
          kToolbarAvatarButtonElementId,
          IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_IPH_TEXT_SIGNIN_REMINDER,
          IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_IPH_TEXT_SIGNIN_REMINDER_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo(IDC_SHOW_AVATAR_MENU))
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kKeyedNotice)
          .SetBubbleTitleText(
              IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_IPH_TITLE_SIGNIN_REMINDER)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleIcon(&vector_icons::kCelebrationIcon)
          .SetReshowPolicy(base::Days(14), /*max_show_count=*/6)));
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // kIPHPwaQuietNotificationFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHPwaQuietNotificationFeature,
          kNotificationContentSettingImageView, IDS_QUIET_NOTIFICATION_IPH_TEXT,
          IDS_QUIET_NOTIFICATION_IPH_TEXT_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kKeyedNotice)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetReshowPolicy(base::Days(100), /*max_show_count=*/5)
          .SetMetadata(80, "lyf@chromium.org",
                       "Triggered once per-app when is in quiet notification "
                       "mode and a notification is triggered in a PWA.")));

  // kIPHCookieControlsFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHCookieControlsFeature,
          kCookieControlsIconElementId, IDS_COOKIE_CONTROLS_PROMO_TEXT,
          IDS_COOKIE_CONTROLS_PROMO_SEE_HOW_BUTTON_TEXT,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                if (IsPageActionMigrated(PageActionIconType::kCookieControls)) {
                  actions::ActionManager::Get()
                      .FindAction(
                          kActionShowCookieControls,
                          GetBrowser(ctx)->GetActions()->root_action_item())
                      ->InvokeAction();
                } else {
                  auto* cookie_controls_icon_view =
                      views::ElementTrackerViews::GetInstance()
                          ->GetFirstMatchingViewAs<CookieControlsIconView>(
                              kCookieControlsIconElementId,
                              ctx->GetElementContext());
                  if (cookie_controls_icon_view != nullptr) {
                    cookie_controls_icon_view->ShowCookieControlsBubble();
                  }
                }
              }))
          .SetBubbleTitleText(IDS_COOKIE_CONTROLS_PROMO_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(
              IDS_COOKIE_CONTROLS_PROMO_CLOSE_BUTTON_TEXT)));

  // kIPHReadingListDiscoveryFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHReadingListDiscoveryFeature,
                    kToolbarAppMenuButtonElementId,
                    IDS_READING_LIST_DISCOVERY_PROMO_PINNING)
                    .SetHighlightedMenuItem(
                        ReadingListSubMenuModel::kReadingListMenuShowUI)));

  // kIPHReadingListEntryPointFeature:
  registry.RegisterFeature(FeaturePromoSpecification::CreateForSnoozePromo(
      feature_engagement::kIPHReadingListEntryPointFeature,
      kBookmarkStarViewElementId, IDS_READING_LIST_ENTRY_POINT_PROMO));

  // kIPHReadingListInSidePanelFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHReadingListInSidePanelFeature,
          kToolbarAppMenuButtonElementId,
          IDS_READING_LIST_IN_SIDE_PANEL_PROMO_PINNING)
          .SetHighlightedMenuItem(BookmarkSubMenuModel::kReadingListMenuItem)));

  // kIPHReadingModeSidePanelFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHReadingModeSidePanelFeature,
          kToolbarAppMenuButtonElementId,
          IDS_READING_MODE_SIDE_PANEL_PROMO_PINNING)
          .SetHighlightedMenuItem(ToolsMenuModel::kReadingModeMenuItem)
          .SetMetadata(115, "jocelyntran@chromium.org",
                       "Triggered to encourage users to try out the reading "
                       "mode feature.")));

  // kIPHSideBySidePinnableFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHSideBySidePinnableFeature,
          kToolbarSplitTabsToolbarButtonElementId, IDS_SPLIT_VIEW_BODY_IPH,
          IDS_SPLIT_VIEW_PIN_ACTION_IPH,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                GetBrowser(ctx)->GetProfile()->GetPrefs()->SetBoolean(
                    prefs::kPinSplitTabButton, true);
              }))
          .SetAdditionalConditions(std::move(
              AdditionalConditions().AddAdditionalCondition(AdditionalCondition{
                  feature_engagement::events::kSplitViewCreated,
                  AdditionalConditions::Constraint::kAtLeast, 2})))
          .SetBubbleTitleText(IDS_SPLIT_VIEW_TITLE_IPH)
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          .SetCustomActionDismissText(IDS_NO_THANKS)
          .SetCustomActionIsDefault(true)
          .SetMetadata(
              141, "lugli@google.com",
              "Triggered when user tried to create split view twice.")));

  // kIPHSideBySideTabSwitchFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForTutorialPromo(
          feature_engagement::kIPHSideBySideTabSwitchFeature,
          kTopContainerElementId, IDS_SPLIT_VIEW_TAB_SWITCH_ENTRY_IPH_BODY,
          kSplitViewTutorialId)
          .SetBubbleArrow(HelpBubbleArrow::kNone)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetBubbleTitleText(IDS_SPLIT_VIEW_TAB_SWITCH_ENTRY_IPH_TITLE)
          .SetMetadata(141, "lugli@google.com",
                       "Triggered when user swaps between two tabs three times "
                       "quickly.")));

  // kIPHSidePanelGenericPinnableFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHSidePanelGenericPinnableFeature,
          kSidePanelPinButtonElementId, IDS_SIDE_PANEL_GENERIC_PINNABLE_IPH,
          IDS_SIDE_PANEL_GENERIC_PINNABLE_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetMetadata(121, "corising@chromium.org",
                       "Triggered when a pinnable side panel is opened.")));

  // kIPHSidePanelLensOverlayPinnableFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFeature,
          kSidePanelPinButtonElementId,
          IDS_SIDE_PANEL_LENS_OVERLAY_PINNABLE_IPH,
          IDS_SIDE_PANEL_LENS_OVERLAY_PINNABLE_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
          .SetMetadata(126, "dfried@chromium.org, jdonnelly@google.com",
                       "Triggered when a pinnable lens overlay side panel is "
                       "opened.")));

  // kIPHSidePanelLensOverlayPinnableFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFollowupFeature,
          kPinnedActionToolbarButtonElementId,
          IDS_SIDE_PANEL_LENS_OVERLAY_PINNABLE_FOLLOWUP_IPH,
          IDS_SIDE_PANEL_LENS_OVERLAY_PINNABLE_FOLLOWUP_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleIcon(&vector_icons::kCelebrationIcon)
          .SetMetadata(126, "dfried@chromium.org, jdonnelly@google.com",
                       "Triggered when the lens overlay side panel is pinned.")
          .SetAnchorElementFilter(base::BindRepeating(
              [](const ui::ElementTracker::ElementList& elements)
                  -> ui::TrackedElement* {
                // Locate the action button associated with the Lens Overlay
                // feature. The button must be present in the Actions
                // container in the toolbar.
                for (auto* element : elements) {
                  auto* const button =
                      views::AsViewClass<PinnedActionToolbarButton>(
                          element->AsA<views::TrackedElementViews>()->view());
                  if (button && button->GetActionId() ==
                                    kActionSidePanelShowLensOverlayResults) {
                    return element;
                  }
                }
                return nullptr;
              }))));

  if (features::IsReadAnythingOmniboxChipEnabled()) {
    // kIPHReadingModePageActionLabelFeature:
    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForToastPromo(
            feature_engagement::kIPHReadingModePageActionLabelFeature,
            kOmniboxElementId, IDS_READING_MODE_OMNIBOX_ENTRY_POINT_PROMO,
            IDS_READING_MODE_OMNIBOX_ENTRY_POINT_PROMO_SCREEN_READER,
            FeaturePromoSpecification::AcceleratorInfo())
            .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
            .SetMetadata(144, "kristislee@google.com, komo-eng@google.com",
                         "Triggered when the Reading mode page action appears.")
            .SetAnchorElementFilter(base::BindRepeating(
                [](const ui::ElementTracker::ElementList& elements)
                    -> ui::TrackedElement* {
                  if (elements.empty()) {
                    return nullptr;
                  }
                  BrowserView* const browser_view =
                      views::ElementTrackerViews::GetInstance()
                          ->GetFirstMatchingViewAs<BrowserView>(
                              kBrowserViewElementId, elements[0]->context());
                  IconLabelBubbleView* page_action_view =
                      browser_view->toolbar_button_provider()
                          ->GetPageActionView(kActionSidePanelShowReadAnything);
                  if (!page_action_view || !page_action_view->GetVisible()) {
                    return nullptr;
                  } else {
                    return views::ElementTrackerViews::GetInstance()
                        ->GetElementForView(page_action_view, true);
                  }
                }))));
  }

  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHTabGroupsSaveV2CloseGroupFeature,
          kToolbarAppMenuButtonElementId,
          IDS_SAVED_TAB_GROUPS_V2_INTRO_IPH_APP_MENU_NOT_SYNCED_BODY,
          IDS_SAVED_TAB_GROUPS_V2_INTRO_DEFAULT_BODY_A11Y,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetAnchorElementFilter(
              base::BindRepeating(&tab_groups::SavedTabGroupUtils::
                                      GetAnchorElementForTabGroupsV2IPH))
          .SetBubbleArrowCallback(base::BindRepeating(
              &tab_groups::SavedTabGroupUtils::GetArrowForTabGroupsV2IPH))
          .SetMetadata(127, "dpenning@chromium.org",
                       "triggered on startup when the saved tab groups are "
                       "defaulted to saved for the first time.")));

  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHTabGroupsSaveV2IntroFeature,
          kToolbarAppMenuButtonElementId,
          IDS_WILDCARD,  // Replaced by caller with the correct IDS string.
          IDS_LEARN_MORE,
          CreateNavigationAction(GURL(chrome::kTabGroupsLearnMoreURL)))
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetAnchorElementFilter(
              base::BindRepeating(&tab_groups::SavedTabGroupUtils::
                                      GetAnchorElementForTabGroupsV2IPH))
          .SetBubbleArrowCallback(base::BindRepeating(
              &tab_groups::SavedTabGroupUtils::GetArrowForTabGroupsV2IPH))
          .SetMetadata(127, "dpenning@chromium.org",
                       "triggered on startup when the saved tab groups are "
                       "defaulted to saved for the first time.")));

  if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForCustomAction(
            feature_engagement::kIPHTabGroupsSharedTabChangedFeature,
            kTopContainerElementId, IDS_DATA_SHARING_USER_ED_FIRST_TAB_CHANGE,
            IDS_LEARN_MORE,
            CreateNavigationAction(GURL(
                data_sharing::features::kLearnMoreSharedTabGroupPageURL.Get())))
            .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
            .SetAnchorElementFilter(base::BindRepeating(
                [](const ui::ElementTracker::ElementList& elements)
                    -> ui::TrackedElement* {
                  if (elements.empty()) {
                    return nullptr;
                  }
                  BrowserView* const browser_view =
                      views::ElementTrackerViews::GetInstance()
                          ->GetFirstMatchingViewAs<BrowserView>(
                              kBrowserViewElementId, elements[0]->context());

                  tab_groups::MostRecentSharedTabUpdateStore*
                      most_recent_shared_tab_update_store =
                          browser_view->browser()
                              ->GetFeatures()
                              .most_recent_shared_tab_update_store();

                  if (!most_recent_shared_tab_update_store ||
                      !most_recent_shared_tab_update_store->HasUpdate()) {
                    return nullptr;
                  }

                  return most_recent_shared_tab_update_store->GetIPHAnchor(
                      browser_view);
                }))
            .SetMetadata(
                134, "mickeyburks@google.org",
                "triggered the first time a user updates a shared tab.")));

    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForToastPromo(
            feature_engagement::kIPHTabGroupsSharedTabFeedbackFeature,
            kSharedTabGroupFeedbackElementId,
            IDS_DATA_SHARING_SHARED_GROUPS_FEEDBACK_IPH,
            IDS_DATA_SHARING_SHARED_GROUPS_FEEDBACK_IPH_SCREENREADER,
            FeaturePromoSpecification::AcceleratorInfo())
            .SetMetadata(
                135, "dljames@chromium.org",
                "Triggered when a shared tab becomes the active tab.")));
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // kIPHSupervisedUserProfileSigninFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHSupervisedUserProfileSigninFeature,
          kToolbarAvatarButtonElementId,
          IDS_SUPERVISED_USER_PROFILE_SIGNIN_IPH_TEXT,
          IDS_PROMO_LEARN_MORE_BUTTON,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                // Open parental controls page.
                ShowSingletonTab(
                    GetBrowser(ctx),
                    GURL(supervised_user::kManagedByParentUiMoreInfoUrl));
                base::RecordAction(base::UserMetricsAction(
                    "SupervisedUserProfileSignIn_IPHPromo_"
                    "ParentalControlsPageOpened"));
              }))
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kActionableAlert)
          .SetBubbleIcon(&vector_icons::kFamilyLinkIcon)
          .SetBubbleTitleText(IDS_SUPERVISED_USER_PROFILE_SIGNIN_IPH_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetCustomActionIsDefault(false)
          .SetMetadata(128, "anthie@google.com",
                       "Triggered on signin-in a supervised user to "
                       "a new profile or an existing local profile")));

  // kIPHSignInBenefitsFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHSignInBenefitsFeature,
          kToolbarAvatarButtonElementId, IDS_SIGN_IN_BENEFITS_IPH_TEXT,
          IDS_PROMO_MANAGE_BUTTON,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                // Open account settings page.
                ShowSingletonTab(GetBrowser(ctx),
                                 GURL(chrome::kChromeUIAccountSettingsURL));
              }))
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kActionableAlert)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetCustomActionIsDefault(false)
          .SetMetadata(142, "ddac@google.com",
                       "Triggered for a signed-in user who hasn't turned on "
                       "sync yet, after the sync-to-signin migration.")));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  // kIPHTabOrganizationSuccessFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHTabOrganizationSuccessFeature,
          kTabGroupHeaderElementId, IDS_TAB_ORGANIZATION_SUCCESS_IPH,
          IDS_TAB_ORGANIZATION_SUCCESS_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          .SetMetadata(121, "dpenning@chromium.org",
                       "Triggered when tab organization is accepted.")));

  // kIPHTabSearchFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHTabSearchFeature,
                    kTabSearchButtonElementId, IDS_TAB_SEARCH_PROMO)
                    .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
                    .SetMetadata(92, "tluk@chromium.org",
                                 "Triggered once when there are more than 8 "
                                 "tabs in the tab strip.")));

  // kIPHTabSearchToolbarButtonFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHTabSearchToolbarButtonFeature,
          kTabSearchButtonElementId, IDS_TAB_SEARCH_TOOLBAR_BUTTON_PROMO_BODY,
          IDS_TAB_SEARCH_TOOLBAR_BUTTON_PROMO_BODY,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleIcon(kLightbulbOutlineIcon)
          .SetBubbleTitleText(IDS_TAB_SEARCH_TOOLBAR_BUTTON_PROMO_TITLE)
          .SetMetadata(136, "emshack@chromium.org",
                       "Triggered when the tab search button has been moved "
                       "into the toolbar.")));

  // kIPHDesktopSharedHighlightingFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForLegacyPromo(
                    &feature_engagement::kIPHDesktopSharedHighlightingFeature,
                    kTopContainerElementId, IDS_SHARED_HIGHLIGHTING_PROMO)
                    .SetBubbleArrow(HelpBubbleArrow::kNone)));

  // kIPHWebUiHelpBubbleTestFeature
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForSnoozePromo(
          feature_engagement::kIPHWebUiHelpBubbleTestFeature,
          kWebUIIPHDemoElementIdentifier,
          IDS_PASSWORD_MANAGER_IPH_BODY_SAVE_TO_ACCOUNT)
          .SetBubbleTitleText(IDS_PASSWORD_MANAGER_IPH_TITLE_SAVE_TO_ACCOUNT)
          .SetInAnyContext(true)
          .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
          .SetBubbleIcon(&vector_icons::kCelebrationIcon)
          .SetMetadata(
              90, "dfried@chromium.org",
              "This is a test IPH, designed to verify that IPH can attach to "
              "elements in WebUI in the main browser tab.",
              // These are not required features; they are just an example to
              // ensure that the tester page formats this data correctly.
              Metadata::FeatureSet{
                  &feature_engagement::kIPHWebUiHelpBubbleTestFeature})));

  // kIPHBatterySaverModeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHBatterySaverModeFeature,
          kToolbarBatterySaverButtonElementId,
          IDS_BATTERY_SAVER_MODE_PROMO_TEXT,
          IDS_BATTERY_SAVER_MODE_PROMO_ACTION_TEXT,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* browser = GetBrowser(ctx);
                if (browser) {
                  chrome::ShowSettingsSubPage(browser,
                                              chrome::kPerformanceSubPage);
                }
                RecordBatterySaverIPHOpenSettings(browser != nullptr);
              }))
          .SetBubbleTitleText(IDS_BATTERY_SAVER_MODE_PROMO_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetMetadata(108, "agale@chromium.org",
                       "Triggered when Battery Saver Mode is active.")));

  // kIPHMemorySaverModeFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHMemorySaverModeFeature,
          kToolbarAppMenuButtonElementId, IDS_MEMORY_SAVER_MODE_PROMO_TEXT,
          IDS_MEMORY_SAVER_MODE_PROMO_ACTION_TEXT,
          base::BindRepeating(
              [](ContextPtr context,
                 user_education::FeaturePromoHandle promo_handle) {
                performance_manager::user_tuning::UserPerformanceTuningManager::
                    GetInstance()
                        ->SetMemorySaverModeEnabled(true);
                RecordMemorySaverIPHEnableMode(true);
              }))
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_NO_THANKS)
          .SetBubbleTitleText(IDS_MEMORY_SAVER_MODE_PROMO_TITLE)
          .SetHighlightedMenuItem(ToolsMenuModel::kPerformanceMenuItem)
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kActionableAlert)
          .SetMetadata(108, "agale@chromium.org",
                       "Triggered when device is low on memory.")));

  // kIPHDiscardRingFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDiscardRingFeature, kTabIconElementId,
          IDS_DISCARD_RING_PROMO_TEXT, IDS_DISCARD_RING_PROMO_ACTION_TEXT,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                ShowPromoInPage::Params params;
                params.target_url =
                    chrome::GetSettingsUrl(chrome::kPerformanceSubPage);
                params.bubble_anchor_id = kInactiveTabSettingElementId;
                params.bubble_arrow = HelpBubbleArrow::kBottomRight;
                params.bubble_text =
                    l10n_util::GetStringUTF16(IDS_DISCARD_RING_SETTINGS_TOAST);
                params.close_button_alt_text_id = IDS_CLOSE_PROMO;

                ShowPromoInPage::Start(GetBrowser(ctx), std::move(params));
              }))
          .SetAnchorElementFilter(base::BindRepeating(
              [](const ui::ElementTracker::ElementList& elements)
                  -> ui::TrackedElement* {
                for (auto* element : elements) {
                  auto* tab_icon = views::AsViewClass<TabIcon>(
                      element->AsA<views::TrackedElementViews>()->view());
                  if (tab_icon->GetShowingDiscardIndicator()) {
                    return element;
                  }
                }
                return nullptr;
              }))
          .SetCustomActionDismissText(IDS_PROMO_DISMISS_BUTTON)
          .SetBubbleTitleText(IDS_DISCARD_RING_PROMO_TITLE)
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          // See: crbug.com/358451018
          .OverrideFocusOnShow(false)
          .SetMetadata(126, "agale@chromium.org",
                       "Triggered when a tab is discarded.")));

  // kIPHPriceTrackingInSidePanelFeature;
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForLegacyPromo(
          &feature_engagement::kIPHPriceTrackingInSidePanelFeature,
          kToolbarSidePanelButtonElementId, IDS_PRICE_TRACKING_SIDE_PANEL_IPH)
          .SetMetadata(120, "yuezhanggg@chromium.org",
                       "Triggered when a price tracking is enabled.")));

  // kIPHMerchantTrustFeature
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHMerchantTrustFeature,
                    kMerchantTrustChipElementId, IDS_MERCHANT_TRUST_IPH_BODY,
                    IDS_MERCHANT_TRUST_IPH_BODY_SCREEN_READER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleTitleText(IDS_MERCHANT_TRUST_IPH_TITLE)
                    .SetBubbleIcon(&vector_icons::kStorefrontIcon)
                    .SetMetadata(134, "tommasin@chromium.org",
                                 "Triggered when the merchant trust entry "
                                 "point is shown and expanded.")));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // kIPHDownloadEsbPromoFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDownloadEsbPromoFeature,
          kToolbarDownloadButtonElementId, IDS_DOWNLOAD_BUBBLE_ESB_PROMO,
          IDS_DOWNLOAD_BUBBLE_ESB_PROMO_CUSTOM_ACTION,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                chrome::ShowSafeBrowsingEnhancedProtectionWithIph(
                    GetBrowser(ctx),
                    safe_browsing::SafeBrowsingSettingReferralMethod::
                        kDownloadButtonIphPromo);
              }))
          .SetCustomActionIsDefault(true)
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetBubbleTitleText(IDS_DOWNLOAD_BUBBLE_ESB_PROMO_TITLE)
          .SetCustomActionDismissText(IDS_DOWNLOAD_BUBBLE_ESB_PROMO_DISMISS)
          .SetBubbleIcon(&vector_icons::kGshieldIcon)
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kActionableAlert)
          .SetMetadata(
              122, "awado@chromium.org",
              "Triggered when user is using standard protection mode.")));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // kIPHBackNavigationMenuFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForSnoozePromo(
                    feature_engagement::kIPHBackNavigationMenuFeature,
                    kToolbarBackButtonElementId, IDS_BACK_NAVIGATION_MENU_PROMO,
                    IDS_BACK_NAVIGATION_MENU_PROMO_ACCESSIBLE_TEXT,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kTopLeft)));

  // kIPHLensOverlayTranslateButtonFeature:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForToastPromo(
          feature_engagement::kIPHLensOverlayTranslateButtonFeature,
          kLensOverlayTranslateButtonElementId,
          IDS_LENS_OVERLAY_TRANSLATE_BUTTON_IPH,
          IDS_LENS_OVERLAY_TRANSLATE_BUTTON_IPH_SCREENREADER,
          FeaturePromoSpecification::AcceleratorInfo())
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetInAnyContext(true)
          .SetMetadata(131, "juanmojica@google.com",
                       "Triggered to inform users of the availability of the "
                       "new translate screen feature on the Lens Overlay.")));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  // kIPHDesktopPWAsLinkCapturingLaunch:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch,
          kToolbarAppMenuButtonElementId, IDS_DESKTOP_PWA_LINK_CAPTURING_TEXT,
          IDS_DESKTOP_PWA_LINK_CAPTURING_SETTINGS,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* const browser = GetBrowser(ctx);
                TabStripModel* const tab_strip_model =
                    browser->tab_strip_model();
                if (!tab_strip_model) {
                  return;
                }
                content::WebContents* const web_contents =
                    tab_strip_model->GetActiveWebContents();
                const webapps::AppId* app_id =
                    web_app::WebAppTabHelper::GetAppId(web_contents);
                if (!app_id) {
                  return;
                }
                const GURL final_url(chrome::kChromeUIWebAppSettingsURL +
                                     *app_id);
                if (web_contents) {
                  NavigateParams params(browser->profile(), final_url,
                                        ui::PAGE_TRANSITION_LINK);
                  params.disposition =
                      WindowOpenDisposition::NEW_FOREGROUND_TAB;
                  Navigate(&params);
                }
              }))
          .SetBubbleArrow(HelpBubbleArrow::kTopRight)
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kKeyedNotice)
          .SetMetadata(122, "dibyapal@chromium.org",
                       "Triggered once per-app when a link is captured and "
                       "opened in a PWA.")));

  // kIPHDesktopPWAsLinkCapturingLaunchAppInTab:
  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
          kLocationIconElementId, IDS_DESKTOP_PWA_LINK_CAPTURING_TEXT,
          IDS_DESKTOP_PWA_LINK_CAPTURING_SETTINGS,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                auto* const browser = GetBrowser(ctx);
                TabStripModel* const tab_strip_model =
                    browser->tab_strip_model();
                if (!tab_strip_model) {
                  return;
                }
                content::WebContents* const web_contents =
                    tab_strip_model->GetActiveWebContents();
                const webapps::AppId* app_id =
                    web_app::WebAppTabHelper::GetAppId(web_contents);
                if (!app_id) {
                  return;
                }
                const GURL final_url(chrome::kChromeUIWebAppSettingsURL +
                                     *app_id);
                if (web_contents) {
                  NavigateParams params(browser->profile(), final_url,
                                        ui::PAGE_TRANSITION_LINK);
                  params.disposition =
                      WindowOpenDisposition::NEW_FOREGROUND_TAB;
                  Navigate(&params);
                }
              }))
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          .SetPromoSubtype(
              FeaturePromoSpecification::PromoSubtype::kKeyedNotice)
          .SetMetadata(122, "finnur@chromium.org",
                       "Triggered once per-app when a link is captured and "
                       "opened in a browser tab.")));

  registry.RegisterFeature(std::move(
      FeaturePromoSpecification::CreateForCustomAction(
          feature_engagement::kIPHHistorySearchFeature,
          kHistorySearchInputElementId, IDS_HISTORY_EMBEDDINGS_IPH_BODY,
          IDS_HISTORY_EMBEDDINGS_IPH_ACTION,
          base::BindRepeating(
              [](ContextPtr ctx,
                 user_education::FeaturePromoHandle promo_handle) {
                chrome::ShowSettingsSubPage(GetBrowser(ctx),
                                            chrome::kHistorySearchSubpage);
              }))
          .SetCustomActionIsDefault(true)
          .SetCustomActionDismissText(IDS_NO_THANKS)
          .SetBubbleArrow(HelpBubbleArrow::kTopLeft)
          .SetInAnyContext(true)
          .SetMetadata(130, "johntlee@chromium.org",
                       "Triggered after user lands on chrome://history.")));

  // kIPHiOSLensPromoDesktopFeature
  if (MobilePromoOnDesktopTypeEnabled(
          MobilePromoOnDesktopPromoType::kLensPromo)) {
    registry.RegisterFeature(
        std::move(FeaturePromoSpecification::CreateForCustomUi(
                      feature_engagement::kIPHiOSLensPromoDesktopFeature,
                      kSidePanelElementId,
                      user_education::CreateCustomHelpBubbleViewFactoryCallback(
                          base::BindRepeating(
                              &IOSPromoBubbleView::Create,
                              desktop_to_mobile_promos::PromoType::kLens)))
                      .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
                      .SetMetadata(144, "scottyoder@google.com",
                                   "Triggered when Lens Overlay is used.")));
  }

  // kIPHiOSEnhancedBrowsingDesktopFeature
  if (MobilePromoOnDesktopTypeEnabled(
          MobilePromoOnDesktopPromoType::kESBPromo)) {
    registry.RegisterFeature(std::move(
        FeaturePromoSpecification::CreateForCustomUi(
            feature_engagement::kIPHiOSEnhancedBrowsingDesktopFeature,
            kToolbarAppMenuButtonElementId,
            user_education::CreateCustomHelpBubbleViewFactoryCallback(
                base::BindRepeating(
                    &IOSPromoBubbleView::Create,
                    desktop_to_mobile_promos::PromoType::kEnhancedBrowsing)))
            .SetPromoSubtype(
                FeaturePromoSpecification::PromoSubtype::kActionableAlert)
            .SetMetadata(144, "scottyoder@google.com",
                         "Triggered when ESB is first enabled.")));
  }

  // kIPHiOSPasswordPromoDesktopFeature
  if (MobilePromoOnDesktopTypeEnabled(
          MobilePromoOnDesktopPromoType::kAutofillPromo)) {
    registry.RegisterFeature(
        std::move(FeaturePromoSpecification::CreateForCustomUi(
                      feature_engagement::kIPHiOSPasswordPromoDesktopFeature,
                      kPasswordsOmniboxKeyIconElementId,
                      user_education::CreateCustomHelpBubbleViewFactoryCallback(
                          base::BindRepeating(
                              &IOSPromoBubbleView::Create,
                              desktop_to_mobile_promos::PromoType::kPassword)))
                      .SetMetadata(144, "scottyoder@google.com",
                                   "Triggered when a password is saved.")));
  }

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_COMPOSE)
  // kIPHComposeMSBBSettingsFeature:
  registry.RegisterFeature(
      std::move(FeaturePromoSpecification::CreateForToastPromo(
                    feature_engagement::kIPHComposeMSBBSettingsFeature,
                    kAnonymizedUrlCollectionPersonalizationSettingId,
                    IDS_COMPOSE_MSBB_IPH_BUBBLE_TEXT,
                    IDS_COMPOSE_MSBB_IPH_BUBBLE_TEXT_SCREENREADER,
                    FeaturePromoSpecification::AcceleratorInfo())
                    .SetBubbleArrow(HelpBubbleArrow::kBottomRight)));
#endif  // BUILDFLAG(ENABLE_COMPOSE)
}

void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry) {
  MaybeRegisterChromeFeaturePromos(registry, nullptr);
}

void MaybeRegisterChromeTutorials(
    user_education::TutorialRegistry& tutorial_registry) {
  using user_education::HelpBubbleArrow;
  using user_education::TutorialDescription;
  using BubbleStep = user_education::TutorialDescription::BubbleStep;
  using EventStep = user_education::TutorialDescription::EventStep;
  using HiddenStep = user_education::TutorialDescription::HiddenStep;

  // TODO (dfried): we might want to do something more sophisticated in the
  // future.
  if (tutorial_registry.IsTutorialRegistered(kTabGroupTutorialId)) {
    return;
  }

  {  // Tab Group tutorial.
    auto tab_group_tutorial = TutorialDescription::Create<
        kTabGroupTutorialMetricPrefix>(
        // The initial step. This is the only step that differs depending on
        // whether there is an existing group.
        IfView(kBrowserViewElementId, base::BindRepeating(&HasTabGroups))
            .Then(
                BubbleStep(kTabStripRegionElementId)
                    .SetBubbleBodyText(
                        IDS_TUTORIAL_ADD_TAB_TO_GROUP_WITH_EXISTING_GROUP_IN_TAB_STRIP))
            .Else(BubbleStep(kTabStripRegionElementId)
                      .SetBubbleBodyText(
                          IDS_TUTORIAL_TAB_GROUP_ADD_TAB_TO_GROUP)),

        // Getting the new tab group (hidden step).
        HiddenStep::WaitForShowEvent(kTabGroupHeaderElementId)
            .NameElement(kTabGroupHeaderElementName),

        // The menu step.
        BubbleStep(kTabGroupEditorBubbleId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE)
            .SetBubbleArrow(HelpBubbleArrow::kLeftCenter)
            .AbortIfVisibilityLost(false),

        HiddenStep::WaitForHidden(kTabGroupEditorBubbleId),

        // Drag tab into the group.
        BubbleStep(kTabStripRegionElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_DRAG_TAB),

        EventStep(kTabGroupedCustomEventId).AbortIfVisibilityLost(true),

        // Click to collapse the tab group.
        BubbleStep(kTabGroupHeaderElementName)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_COLLAPSE)
            .SetBubbleArrow(HelpBubbleArrow::kTopCenter),

        HiddenStep::WaitForActivated(kTabGroupHeaderElementId),

        // Completion of the tutorial.
        BubbleStep(kTabStripRegionElementId)
            .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
            .SetBubbleBodyText(IDS_TUTORIAL_TAB_GROUP_SUCCESS_DESCRIPTION));

    tab_group_tutorial.metadata.additional_description =
        "Tutorial for creating new tab groups.";
    tab_group_tutorial.metadata.launch_milestone = 106;
    tab_group_tutorial.metadata.owners = "dpenning@chromium.org";

    tutorial_registry.AddTutorial(kTabGroupTutorialId,
                                  std::move(tab_group_tutorial));
  }

  {  // Side panel customize chrome
    auto customize_chrome_tutorial = TutorialDescription::Create<
        kSidePanelCustomizeChromeTutorialMetricPrefix>(
        // Bubble step - customize chrome button
        BubbleStep(CustomizeButtonsHandler::kCustomizeChromeButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_OPEN_SIDE_PANEL)
            .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
            .InAnyContext(),

        // Bubble step - change theme button
        BubbleStep(CustomizeChromeUI::kChangeChromeThemeButtonElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_CHANGE_THEME)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
            .AbortIfVisibilityLost(false)
            .InAnyContext(),

        // Bubble step - select collection
        BubbleStep(CustomizeChromeUI::kChromeThemeCollectionElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_SELECT_COLLECTION)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
            .AbortIfVisibilityLost(false)
            .InAnyContext(),

        // Bubble step - select theme
        BubbleStep(CustomizeChromeUI::kChromeThemeElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_APPLY_THEME)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
            .AbortIfVisibilityLost(false)
            .InAnyContext(),

        // Event step - select theme event
        EventStep(kBrowserThemeChangedEventId, kBrowserViewElementId),

        // Bubble step - back button
        BubbleStep(CustomizeChromeUI::kChromeThemeBackElementId)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_CLICK_BACK_ARROW)
            .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
            .NameElement(kChromeThemeBackElementName)
            .AbortIfVisibilityLost(false)
            .InAnyContext(),

        // Hidden step - back button
        HiddenStep::WaitForHidden(kChromeThemeBackElementName),

        // Completion of the tutorial.
        BubbleStep(CustomizeButtonsHandler::kCustomizeChromeButtonElementId)
            .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
            .SetBubbleArrow(HelpBubbleArrow::kBottomRight)
            .SetBubbleBodyText(IDS_TUTORIAL_CUSTOMIZE_CHROME_SUCCESS_BODY)
            .InAnyContext());

    customize_chrome_tutorial.metadata.additional_description =
        "Tutorial for customizing themes using side panel.";
    customize_chrome_tutorial.metadata.launch_milestone = 143;
    customize_chrome_tutorial.metadata.owners = "rsult@google.com";

    tutorial_registry.AddTutorial(kSidePanelCustomizeChromeTutorialId,
                                  std::move(customize_chrome_tutorial));
  }

  {  // Password Manager tutorial
    auto password_manager_tutorial =
        TutorialDescription::Create<kPasswordManagerTutorialMetricPrefix>(
            // Bubble step - Browser app menu
            BubbleStep(kToolbarAppMenuButtonElementId)
                .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_OPEN_APP_MENU)
                .SetBubbleArrow(HelpBubbleArrow::kTopRight),

            // Wait for one of the next elements so the If step can check
            // for the optional element.
            TutorialDescription::WaitForAnyOf(
                AppMenuModel::kPasswordAndAutofillMenuItem)
                .Or(AppMenuModel::kPasswordManagerMenuItem),

            TutorialDescription::If(AppMenuModel::kPasswordAndAutofillMenuItem)
                .Then(
                    // Bubble step - Passwords and Autofill sub menu item
                    BubbleStep(AppMenuModel::kPasswordAndAutofillMenuItem)
                        .SetBubbleBodyText(
                            IDS_TUTORIAL_PASSWORD_MANAGER_CLICK_PASSWORDS_MENU)
                        .SetBubbleArrow(HelpBubbleArrow::kRightCenter)),

            // Bubble step - "Password Manager" menu item
            BubbleStep(AppMenuModel::kPasswordManagerMenuItem)
                .SetBubbleBodyText(
                    IDS_TUTORIAL_PASSWORD_MANAGER_CLICK_PASSWORD_MANAGER)
                .SetBubbleArrow(HelpBubbleArrow::kRightCenter)
                .AbortIfVisibilityLost(false),

            // Bubble step - "Add shortcut" row
            BubbleStep(PasswordManagerUI::kAddShortcutElementId)
                .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_ADD_SHORTCUT)
                .SetBubbleArrow(HelpBubbleArrow::kTopCenter)
                .InAnyContext(),

            // Event step - Click on "Add shortcut"
            EventStep(PasswordManagerUI::kAddShortcutCustomEventId)
                .InSameContext(),

            // Bubble step - "Install" row
            BubbleStep(web_app::WebAppInstallDialogDelegate::
                           kPwaInstallDialogInstallButton)
                .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_CLICK_INSTALL)
                .SetBubbleArrow(HelpBubbleArrow::kTopRight),

            // Event step - Click on "Install"
            EventStep(
                web_app::WebAppInstallDialogDelegate::kInstalledPWAEventId)
                .InSameContext(),

            // Completion of the tutorial.
            BubbleStep(kTopContainerElementId)
                .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
                .SetBubbleBodyText(IDS_TUTORIAL_PASSWORD_MANAGER_SUCCESS_BODY)
                .SetBubbleArrow(HelpBubbleArrow::kNone));

    password_manager_tutorial.metadata.additional_description =
        "Tutorial for installing password manager.";
    password_manager_tutorial.metadata.launch_milestone = 116;
    password_manager_tutorial.metadata.owners = "mickeyburks@chromium.org";

    tutorial_registry.AddTutorial(kPasswordManagerTutorialId,
                                  std::move(password_manager_tutorial));
  }

  {  // Lens Overlay tutorial
    auto lens_overlay_tutorial =
        TutorialDescription::Create<kLensOverlayTutorialMetricPrefix>(

            // Bubble step - Address bar
            BubbleStep(kOmniboxElementId)
                .SetBubbleBodyText(IDS_TUTORIAL_LENS_OVERLAY_CLICK_ADDRESS_BAR)
                .SetBubbleArrow(HelpBubbleArrow::kTopCenter),

            // Bubble step - Lens button
            BubbleStep(kLensOverlayPageActionIconElementId)
                .SetBubbleBodyText(
                    IDS_TUTORIAL_LENS_OVERLAY_HOMEWORK_CLICK_LENS)
                .SetBubbleArrow(HelpBubbleArrow::kTopRight),

            // Lens button hides when clicked
            HiddenStep::WaitForHidden(kLensOverlayPageActionIconElementId),

            // Completion of the tutorial after side panel appears.
            BubbleStep(kLensSidePanelSearchBoxElementId)
                .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
                .SetBubbleBodyText(IDS_TUTORIAL_LENS_OVERLAY_CLICK_SEARCH_BOX)
                .SetBubbleArrow(HelpBubbleArrow::kLeftTop)
                .InAnyContext());

    lens_overlay_tutorial.metadata.additional_description =
        "Tutorial for the Lens Overlay.";
    lens_overlay_tutorial.metadata.launch_milestone = 131;
    lens_overlay_tutorial.metadata.owners = "nguyenbryan@google.com";

    tutorial_registry.AddTutorial(kLensOverlayTutorialId,
                                  std::move(lens_overlay_tutorial));
  }

  {  // Split view tutorial
    auto split_view_tutorial =
        TutorialDescription::Create<kSplitViewTutorialMetricPrefix>(
            // Hidden step - name the last inactive tab
            HiddenStep::WaitForShown(kBrowserViewElementId)
                .NameElements(
                    base::BindRepeating([](ui::InteractionSequence* sequence,
                                           ui::TrackedElement* element) {
                      BrowserView* const browser_view =
                          views::AsViewClass<BrowserView>(
                              element->AsA<views::TrackedElementViews>()
                                  ->view());

                      SplitViewIphController* split_view_iph_controller =
                          SplitViewIphController::From(browser_view->browser());

                      ui::TrackedElement* last_inactive_tab_element =
                          split_view_iph_controller->GetTabSwitchIPHAnchor(
                              browser_view);

                      sequence->NameElement(
                          last_inactive_tab_element,
                          std::string_view(kLastInactiveTabElementName));
                      return true;
                    })),

            // Bubble step - inactive tab to right click.
            BubbleStep(kLastInactiveTabElementName)
                .SetBubbleBodyText(IDS_SPLIT_VIEW_TAB_SWITCH_STEP_IPH_BODY)
                .SetBubbleArrow(HelpBubbleArrow::kTopLeft),

            HiddenStep::WaitForShown(kToolbarSplitTabsToolbarButtonElementId),

            // Bubble step - highlight the toolbar button.
            BubbleStep(kToolbarSplitTabsToolbarButtonElementId)
                .SetBubbleBodyText(IDS_SPLIT_VIEW_TOOLBAR_BUTTON_STEP_IPH_BODY)
                .SetBubbleArrow(HelpBubbleArrow::kTopLeft),

            HiddenStep::WaitForShown(
                SplitTabMenuModel::kReversePositionMenuItem),

            // Completion of the tutorial after split view appears.
            BubbleStep(SplitTabMenuModel::kExitSplitMenuItem)
                .SetBubbleTitleText(IDS_TUTORIAL_GENERIC_SUCCESS_TITLE)
                .SetBubbleBodyText(
                    IDS_SPLIT_VIEW_TAB_SWITCH_COMPLETION_IPH_BODY)
                .SetBubbleArrow(HelpBubbleArrow::kLeftTop)
                .InAnyContext());

    split_view_tutorial.metadata.additional_description =
        "Tutorial for the Split View.";
    split_view_tutorial.metadata.launch_milestone = 141;
    split_view_tutorial.metadata.owners = "lugli@google.com";

    tutorial_registry.AddTutorial(kSplitViewTutorialId,
                                  std::move(split_view_tutorial));
  }
}

// NOTES FOR FEATURE TEAMS:
//
// 1. If you add a badge here, be sure to add the name of the corresponding
//    feature to tools/metrics/histograms/metadata/user_education/histograms.xml
//
// 2. When a feature ship and you are removing the feature flag, you must also
//    remove the entry here. THIS IS BY DESIGN. This is a point at which the
//    feature is no longer "new", even for holdback users (at least by the time
//    the code change rolls out to Stable). DO NOT keep a feature flag around
//    longer that necessary just to keep a "New" Badge around.
//
void MaybeRegisterChromeNewBadges(user_education::NewBadgeRegistry& registry) {
  if (registry.IsFeatureRegistered(
          user_education::features::kNewBadgeTestFeature)) {
    return;
  }

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      user_education::features::kNewBadgeTestFeature,
      user_education::Metadata(124, "Frizzle Team",
                               "Used to test \"New\" Badge logic.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kDevToolsAiAssistanceFileAgent,
      user_education::Metadata(132, "wolfi@chromium.org, kimanh@chromium.org",
                               "Shown in the Sources panel in the AI menu item "
                               "when opening the context menu of a file.")));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kDevToolsAiAssistanceNetworkAgent,
      user_education::Metadata(
          132, "wolfi@chromium.org, kimanh@chromium.org",
          "Shown in the Network panel in the AI menu item "
          "when opening the context menu of a network request.")));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kDevToolsAiAssistancePerformanceAgent,
      user_education::Metadata(
          132, "jacktfranklin@chromium.org, kimanh@chromium.org",
          "Shown in the Performance panel in the AI menu item "
          "when opening the context menu of a main thread task.")));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kDevToolsFreestyler,
      user_education::Metadata(
          131, "wolfi@chromium.org, kimanh@chromium.org",
          "Shown in the Elements panel in the AI menu item "
          "when opening the context menu of a DOM element.")));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kDevToolsAiSubmenuPrompts,
      user_education::Metadata(
          142, "kprokopenko@chromium.org, kimanh@chromium.org",
          "Shows AI submenu prompts in the AI menu item.")));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kDevToolsAiDebugWithAi,
      user_education::Metadata(
          142, "kprokopenko@chromium.org, kimanh@chromium.org",
          "Shows Debug with AI menu item for AI assistance.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      compose::features::kEnableCompose,
      user_education::Metadata(124, "dewittj@chromium.org",
                               "Shown in Help Me Write context menu item.", {},
                               kComposePlatforms)));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      compose::features::kEnableComposeSavedStateNudge,
      user_education::Metadata(124, "dewittj@chromium.org",
                               "Shown in autofill-style suggestion UI to "
                               "resume an ongoing Compose session.",
                               {}, kComposePlatforms)));
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      compose::features::kEnableComposeProactiveNudge,
      user_education::Metadata(126, "dewittj@chromium.org",
                               "Shown in autofill-style suggestion UI when "
                               "Compose proactive nudge is shown.",
                               {}, kComposePlatforms)));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      lens::features::kLensOverlay,
      user_education::Metadata(126, "jdonnelly@google.com, dfried@google.com",
                               "Shown in app and web context menus.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      plus_addresses::features::kPlusAddressFallbackFromContextMenu,
      user_education::Metadata(
          128, "jkeitel@google.com",
          "Shown in the autofill section of the context menu where manual "
          "fallback for plus addresses is offered.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      plus_addresses::features::kPlusAddressesEnabled,
      user_education::Metadata(128, "jkeitel@google.com",
                               "Shown in the autofill popup for suggestions to "
                               "create a new plus address.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      password_manager::features::kPasswordManualFallbackAvailable,
      user_education::Metadata(
          128, "brunobraga@google.com",
          "For passwords manual fallback; shown in the context menu.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kTabstripDeclutter,
      user_education::Metadata(
          132, "emshack@chromium.org",
          "Shown in app menu when Tab Declutter menu item is enabled.")));

#if BUILDFLAG(ENABLE_GLIC)
  // This is a custom UI new badge that uses a small help bubble to annotate the
  // element instead of a badge.
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kGlic,
      // TODO(crbug.com/391699323): fill in launch milestone
      user_education::Metadata(136, "agale@chromium.org",
                               "Shown in the glic settings page when the user "
                               "wants to change the toggle value.")));

  // This is a custom UI new badge that uses a small help bubble to annotate the
  // element instead of a badge.
  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kGlicKeyboardShortcutNewBadge,
      // TODO(crbug.com/391699323): fill in launch milestone
      user_education::Metadata(136, "agale@chromium.org",
                               "Shown in the glic settings page when the user "
                               "wants to change the keyboard shortcut.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kGlicAppMenuNewBadge,
      user_education::Metadata(136, "sophey@chromium.org",
                               "Shown in the three dot menu.")));
#endif  // BUILDFLAG(ENABLE_GLIC)

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kSideBySide,
      user_education::Metadata(
          141, "emshack@chromium.org",
          "Shown in the tab context menu when the user enters or exits split "
          "view.")));

  registry.RegisterFeature(user_education::NewBadgeSpecification(
      features::kSideBySideLinkMenuNewBadge,
      user_education::Metadata(141, "emshack@chromium.org",
                               "Shown in the link context menu to open the "
                               "link in a new split tab.")));
}

std::unique_ptr<user_education::FeaturePromoControllerCommon>
CreateUserEducationResources(UserEducationService& user_education_service) {
  Profile* const profile = &user_education_service.profile();
  CHECK(UserEducationServiceFactory::ProfileAllowsUserEducation(profile));

  // Consider registering factories, etc.
  RegisterChromeHelpBubbleFactories(
      user_education_service.help_bubble_factory_registry());
  MaybeRegisterChromeFeaturePromos(
      user_education_service.feature_promo_registry(), profile);
  MaybeRegisterChromeTutorials(user_education_service.tutorial_registry());
  CHECK(user_education_service.new_badge_registry());

  MaybeRegisterChromeNewBadges(*user_education_service.new_badge_registry());
  user_education_service.new_badge_controller()->InitData();

  // Registry is valid if the NTP promo feature is enabled.
  if (user_education_service.ntp_promo_registry()) {
    MaybeRegisterNtpPromos(*user_education_service.ntp_promo_registry());
  }

  if (user_education::features::IsUserEducationV25()) {
    auto result = std::make_unique<BrowserFeaturePromoController25>(
        feature_engagement::TrackerFactory::GetForBrowserContext(profile),
        &user_education_service.feature_promo_registry(),
        &user_education_service.help_bubble_factory_registry(),
        &user_education_service.user_education_storage_service(),
        &user_education_service.feature_promo_session_policy(),
        &user_education_service.tutorial_service(),
        &user_education_service.product_messaging_controller());
    result->Init();
    return result;
  } else {
    return std::make_unique<BrowserFeaturePromoController20>(
        feature_engagement::TrackerFactory::GetForBrowserContext(profile),
        &user_education_service.feature_promo_registry(),
        &user_education_service.help_bubble_factory_registry(),
        &user_education_service.user_education_storage_service(),
        &user_education_service.feature_promo_session_policy(),
        &user_education_service.tutorial_service(),
        &user_education_service.product_messaging_controller());
  }
}

void QueueLegalAndPrivacyNotices(Profile* profile) {
  // Privacy Sandbox Notice
  if (auto* privacy_sandbox_service =
          PrivacySandboxServiceFactory::GetForProfile(profile)) {
    privacy_sandbox_service->GetPrivacySandboxNoticeQueueManager()
        .MaybeQueueNotice();
  }
}
