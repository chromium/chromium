// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/customize_chrome/side_panel_controller_base.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#else
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"  // nogncheck
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"  // nogncheck
#endif

namespace customize_chrome {

SidePanelControllerBase::SidePanelControllerBase(tabs::TabInterface& tab)
    : tab_(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  content::WebContentsObserver::Observe(tab_->GetContents());
  will_discard_contents_callback_subscription_ =
      tab_->RegisterWillDiscardContents(
          base::BindRepeating(&SidePanelControllerBase::WillDiscardContents,
                              base::Unretained(this)));
}

SidePanelControllerBase::~SidePanelControllerBase() = default;

bool SidePanelControllerBase::IsCustomizeChromeEntryAvailable() const {
  auto* registry = SidePanelRegistry::From(&tab_.get());
  return registry && registry->GetEntryForKey(SidePanelEntry::Key(
                         SidePanelEntry::Id::kCustomizeChrome));
}

bool SidePanelControllerBase::IsCustomizeChromeEntryShowing() const {
  SidePanelUI* side_panel_ui = GetSidePanelUI();
  return side_panel_ui &&
         side_panel_ui->IsSidePanelEntryShowing(
             SidePanelEntryKey(SidePanelEntry::Id::kCustomizeChrome));
}

void SidePanelControllerBase::OnEntryShown(SidePanelEntry* entry) {
  if (entry_state_changed_callback_) {
    entry_state_changed_callback_.Run(/*is_showing=*/true);
  }
}

void SidePanelControllerBase::OnEntryHidden(SidePanelEntry* entry) {
  if (entry_state_changed_callback_) {
    entry_state_changed_callback_.Run(/*is_showing=*/false);
  }
}

void SidePanelControllerBase::OnEntryWillHide(SidePanelEntry* entry,
                                              SidePanelEntryHideReason reason) {
  // Only count explicit user action to close the SidePanel here.
  // The SidePanel may be hidden if the user opens a new tab or navigates away
  // without explicitly closing it. In those cases the view of the SidePanel
  // still exists, therefore we do not count those events. Also closing the
  // browser with an opened SidePanel does not trigger this call.
  if (reason == SidePanelEntryHideReason::kSidePanelClosed) {
    Profile* const profile = tab_->GetProfile();
    profile->GetPrefs()->SetBoolean(prefs::kNtpCustomizeChromeExplicitlyClosed,
                                    true);
  }
}

bool SidePanelControllerBase::CanShowOnURL(const GURL& url) const {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/507919199): CustomizeChromePageHandler is not compiled on
  // Android, so its support check is not available here yet.
  return true;
#else
  Profile* const profile = tab_->GetProfile();
  return CustomizeChromePageHandler::IsSupported(
      NtpCustomBackgroundServiceFactory::GetForProfile(profile), profile);
#endif
}

void SidePanelControllerBase::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Check the actual navigation entry of the page, this is more proper than the
  // navigation handles information since the navigation handle can include
  // several other navigation types.
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    entry = web_contents()->GetController().GetVisibleEntry();
  }
  if (!entry) {
    return;
  }
  const GURL& url = entry->GetURL();
  if (!CanShowOnURL(url)) {
    return;
  }
  CreateAndRegisterEntry();
  OnEntryRegisteredForUrl(url);
}

void SidePanelControllerBase::CreateAndRegisterEntry() {
  auto* registry = SidePanelRegistry::From(&tab_.get());
  if (!registry) {
    return;
  }
  // If the registry already has an entry then disregard.
  if (registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome))) {
    return;
  }
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome),
      base::BindRepeating(&SidePanelControllerBase::CreateCustomizeChromeView,
                          base::Unretained(this)),
      /*default_content_width_callback=*/base::NullCallback());
#if BUILDFLAG(IS_ANDROID)
  entry->SetProperty(
      kSidePanelTitleKey,
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE));
#endif
  entry->AddObserver(this);
  registry->Register(std::move(entry));
}

void SidePanelControllerBase::OpenSidePanel(
    SidePanelOpenTrigger trigger,
    std::optional<CustomizeChromeSection> /*section*/) {
  SidePanelUI* side_panel_ui = GetSidePanelUI();
  if (side_panel_ui) {
    side_panel_ui->Show(SidePanelEntry::Id::kCustomizeChrome, trigger);
  }
}

void SidePanelControllerBase::CloseSidePanel() {
  if (!IsCustomizeChromeEntryShowing()) {
    return;
  }
  SidePanelUI* side_panel_ui = GetSidePanelUI();
  if (side_panel_ui) {
    side_panel_ui->Close();
  }
}

SidePanelUI* SidePanelControllerBase::GetSidePanelUI() const {
  auto* browser = tab_->GetBrowserWindowInterface();
  return browser ? SidePanelUIProvider::From(browser) : nullptr;
}

void SidePanelControllerBase::SetEntryChangedCallback(
    StateChangedCallBack callback) {
  entry_state_changed_callback_ = std::move(callback);
}

void SidePanelControllerBase::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* previous_contents,
    content::WebContents* new_contents) {
  content::WebContentsObserver::Observe(new_contents);
}

}  // namespace customize_chrome
