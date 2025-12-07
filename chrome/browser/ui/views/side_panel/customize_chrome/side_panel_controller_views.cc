// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/search/ntp_features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_CustomizeChromeUI =
    SidePanelWebUIViewT<CustomizeChromeUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT<CustomizeChromeUI>,
                        SidePanelWebUIViewT)
END_METADATA

namespace {

constexpr SidePanelEntry::Id kSidePanelEntryId =
    SidePanelEntry::Id::kCustomizeChrome;

}  // anonymous namespace

namespace customize_chrome {

SidePanelControllerViews::SidePanelControllerViews(tabs::TabInterface& tab)
    : tab_(tab) {
  content::WebContentsObserver::Observe(tab_->GetContents());
  will_discard_contents_callback_subscription_ =
      tab_->RegisterWillDiscardContents(
          base::BindRepeating(&SidePanelControllerViews::WillDiscardContents,
                              base::Unretained(this)));
}

SidePanelControllerViews::~SidePanelControllerViews() = default;

bool SidePanelControllerViews::IsCustomizeChromeEntryShowing() const {
  auto* side_panel_ui = GetSidePanelUI();
  return side_panel_ui && side_panel_ui->IsSidePanelEntryShowing(
                              SidePanelEntryKey(kSidePanelEntryId));
}

bool SidePanelControllerViews::IsCustomizeChromeEntryAvailable() const {
  auto* registry = tab_->GetTabFeatures()->side_panel_registry();
  return registry ? (registry->GetEntryForKey(
                         SidePanelEntry::Key(kSidePanelEntryId)) != nullptr)
                  : false;
}

void SidePanelControllerViews::OnEntryShown(SidePanelEntry* entry) {
  if (entry_state_changed_callback_) {
    entry_state_changed_callback_.Run(true);
  }
}

void SidePanelControllerViews::OnEntryHidden(SidePanelEntry* entry) {
  if (entry_state_changed_callback_) {
    entry_state_changed_callback_.Run(false);
  }
}

void SidePanelControllerViews::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  // Only count explicit user action to close the SidePanel here.
  // The SidePanel may be hidden if the user opens a new tab or navigates away
  // without explicitly closing the it. In those cases the view of the SidePanel
  // still exists, therefore we do not count those events. Also closing the
  // browser with an opened SidePanel does not trigger this call.
  // TODO(crbug.com/454215211): Follow up this check with more robust solution.
  if (reason == SidePanelEntryHideReason::kSidePanelClosed &&
      !entry->CachedView()) {
    Profile* const profile =
        Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
    profile->GetPrefs()->SetBoolean(prefs::kNtpCustomizeChromeExplicitlyClosed,
                                    true);
  }
}

bool SidePanelControllerViews::CanShowOnURL(const GURL& url) const {
  // Check to make sure that all of the required services work for the
  // CustomizeChromePageHandler

  Profile* const profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());

  if (!CustomizeChromePageHandler::IsSupported(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile), profile)) {
    return false;
  }

  return true;
}

bool SidePanelControllerViews::ShouldEnableEditTheme(const GURL& url) const {
  Profile* const profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return NewTabPageUI::IsNewTabPageOrigin(url) ||
         (base::FeatureList::IsEnabled(ntp_features::kNtpFooter) &&
          ntp_footer::IsExtensionNtp(url, profile));
}

void SidePanelControllerViews::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Check the actual navigation entry of the page, this is more proper than the
  // navigation handles information since the navigation handle can include
  // several other navigation types.
  content::NavigationEntry* entry =
      tab_->GetContents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    entry = tab_->GetContents()->GetController().GetVisibleEntry();
  }

  const GURL& url = entry->GetURL();
  if (CanShowOnURL(url)) {
    CreateAndRegisterEntry();
    if (customize_chrome_ui_) {
      customize_chrome_ui_->AttachedTabStateUpdated(url);
      customize_chrome_ui_->UpdateThemeEditable(ShouldEnableEditTheme(url));
    }
  } else {
    DeregisterEntry();
  }
}

void SidePanelControllerViews::CreateAndRegisterEntry() {
  auto* registry = tab_->GetTabFeatures()->side_panel_registry();

  if (!registry) {
    return;
  }

  // If the registry already has an entry then disregard.
  if (registry->GetEntryForKey(SidePanelEntry::Key(kSidePanelEntryId))) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(kSidePanelEntryId),
      base::BindRepeating(
          &SidePanelControllerViews::CreateCustomizeChromeWebView,
          base::Unretained(this)),
      /*default_content_width_callback=*/base::NullCallback());
  entry->AddObserver(this);
  registry->Register(std::move(entry));
}

void SidePanelControllerViews::DeregisterEntry() {
  auto* registry = tab_->GetTabFeatures()->side_panel_registry();

  if (!registry) {
    return;
  }

  auto* current_entry =
      registry->GetEntryForKey(SidePanelEntry::Key(kSidePanelEntryId));
  if (!current_entry) {
    return;
  }

  current_entry->RemoveObserver(this);
  registry->Deregister(SidePanelEntry::Key(kSidePanelEntryId));
}

void SidePanelControllerViews::OpenSidePanel(
    SidePanelOpenTrigger trigger,
    std::optional<CustomizeChromeSection> section) {
  SidePanelUI* side_panel_ui = GetSidePanelUI();
  if (!side_panel_ui) {
    return;
  }

  DCHECK(IsCustomizeChromeEntryAvailable());

  // SidePanelUI::Show calls into CreateCustomizeChromeWebView which sets up
  // the customize_chrome_ui_.
  side_panel_ui->Show(kSidePanelEntryId, trigger);
  if (customize_chrome_ui_ && section.has_value()) {
    customize_chrome_ui_->ScrollToSection(section.value());
    section.reset();
  } else {
    section_ = section;
  }
}

void SidePanelControllerViews::CloseSidePanel() {
  // If the CustomizeChromeUI isn't showing already, dont do anything.
  if (!IsCustomizeChromeEntryShowing()) {
    return;
  }

  auto* const registry = tab_->GetTabFeatures()->side_panel_registry();
  auto* const current_entry =
      registry->GetEntryForKey(SidePanelEntry::Key(kSidePanelEntryId));
  if (!current_entry) {
    return;
  }

  GetSidePanelUI()->Close(current_entry->type());
}

std::unique_ptr<views::View>
SidePanelControllerViews::CreateCustomizeChromeWebView(
    SidePanelEntryScope& scope) {
  auto customize_chrome_web_view =
      std::make_unique<SidePanelWebUIViewT<CustomizeChromeUI>>(
          scope, base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<WebUIContentsWrapperT<CustomizeChromeUI>>(
              GURL(chrome::kChromeUICustomizeChromeSidePanelURL),
              Profile::FromBrowserContext(
                  tab_->GetContents()->GetBrowserContext()),
              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
              /*esc_closes_ui=*/false));
  customize_chrome_web_view->SetProperty(
      views::kElementIdentifierKey, kCustomizeChromeSidePanelWebViewElementId);
  customize_chrome_web_view->ShowUI();
  customize_chrome_ui_ = customize_chrome_web_view->contents_wrapper()
                             ->GetWebUIController()
                             ->GetWeakPtr();
  if (section_.has_value()) {
    customize_chrome_ui_->ScrollToSection(*section_);
    section_.reset();
  }

  // Immediately apply the tab's state to the customize chrome UI.
  content::NavigationEntry* entry =
      tab_->GetContents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    entry = tab_->GetContents()->GetController().GetVisibleEntry();
  }
  const GURL& url = entry->GetURL();
  customize_chrome_ui_->AttachedTabStateUpdated(url);
  customize_chrome_ui_->UpdateThemeEditable(ShouldEnableEditTheme(url));

  return customize_chrome_web_view;
}

SidePanelUI* SidePanelControllerViews::GetSidePanelUI() const {
  return tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_ui();
}

void SidePanelControllerViews::SetEntryChangedCallback(
    StateChangedCallBack callback) {
  entry_state_changed_callback_ = std::move(callback);
}

void SidePanelControllerViews::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* previous_contents,
    content::WebContents* new_contents) {
  content::WebContentsObserver::Observe(new_contents);
}

}  // namespace customize_chrome
