// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace extensions {

namespace {

int GetCurrentTabId(Browser* browser) {
  return ExtensionTabUtil::GetTabId(
      browser->tab_strip_model()->GetActiveWebContents());
}

bool IsSidePanelEnabled(const api::side_panel::PanelOptions& options) {
  return options.enabled.has_value() && *options.enabled &&
         options.path.has_value();
}

bool HasGlobalSidePanel(content::BrowserContext* context,
                        const Extension& extension) {
  auto options = SidePanelService::Get(context)->GetOptions(
      extension, /*tab_id=*/std::nullopt);
  return IsSidePanelEnabled(options);
}

}  // namespace

ExtensionSidePanelCoordinator::ExtensionSidePanelCoordinator(
    Profile* profile,
    Browser* browser,
    content::WebContents* web_contents,
    const Extension* extension,
    SidePanelRegistry* registry,
    bool for_tab)
    : profile_(profile),
      browser_(browser),
      web_contents_(web_contents),
      extension_(extension),
      registry_(registry),
      for_tab_(for_tab) {
  // Only one of `browser` or `web_contents` should be defined when constructing
  // this class.
  DCHECK(browser != nullptr ^ web_contents != nullptr);

  // The registry should always be available for this class.
  DCHECK(registry_);

  SidePanelService* service = SidePanelService::Get(profile);
  // `service` can be null for some tests.
  if (service) {
    scoped_service_observation_.Observe(service);
    LoadExtensionIcon();
    if (IsGlobalCoordinator()) {
      UpdateActionItemIcon();
      browser_->tab_strip_model()->AddObserver(this);
    }

    auto options =
        IsGlobalCoordinator()
            ? service->GetOptions(*extension, /*tab_id=*/std::nullopt)
            : service->GetSpecificOptionsForTab(
                  *extension, ExtensionTabUtil::GetTabId(web_contents));
    if (IsSidePanelEnabled(options)) {
      side_panel_url_ = extension->GetResourceURL(*options.path);
      CreateAndRegisterEntry();
    }
  }
}

ExtensionSidePanelCoordinator::~ExtensionSidePanelCoordinator() = default;

content::WebContents*
ExtensionSidePanelCoordinator::GetHostWebContentsForTesting() const {
  DCHECK(host_);
  return host_->host_contents();
}

SidePanelEntry::Key ExtensionSidePanelCoordinator::GetEntryKey() const {
  return SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_->id());
}

SidePanelEntry* ExtensionSidePanelCoordinator::GetEntry() const {
  return registry_->GetEntryForKey(GetEntryKey());
}

bool ExtensionSidePanelCoordinator::IsGlobalCoordinator() const {
  return browser_ != nullptr;
}

bool ExtensionSidePanelCoordinator::IsDisabledForTab(int tab_id) const {
  auto options =
      SidePanelService::Get(profile_)->GetOptions(*extension_, tab_id);
  return options.enabled.has_value() && !(*options.enabled);
}

void ExtensionSidePanelCoordinator::DeregisterEntry() {
  registry_->Deregister(GetEntryKey());

  global_entry_view_.reset();
}

void ExtensionSidePanelCoordinator::DeregisterGlobalEntryAndCacheView() {
  CHECK(IsGlobalCoordinator());
  if (GetEntry()) {
    global_entry_view_ =
        SidePanelUtil::DeregisterAndReturnView(registry_, GetEntryKey());
  }
}

void ExtensionSidePanelCoordinator::OnPanelOptionsChanged(
    const ExtensionId& extension_id,
    const api::side_panel::PanelOptions& updated_options) {
  // Ignore all changes that are not for this extension.
  if (extension_id != extension_->id()) {
    return;
  }

  bool should_enable_entry =
      updated_options.enabled.has_value() && *updated_options.enabled;
  bool should_disable_entry =
      updated_options.enabled.has_value() && !(*updated_options.enabled);
  SidePanelEntry* entry = GetEntry();

  // PanelOptions changes for the current tab can also affect the global
  // SidePanelEntry. Handle such cases here.
  if (IsGlobalCoordinator() &&
      GetCurrentTabId(browser_) == updated_options.tab_id) {
    if (!entry && should_enable_entry &&
        HasGlobalSidePanel(profile_, *extension_)) {
      // We create an entry if:
      //  - The side panel is being enabled/no longer being disabled for this
      //    tab
      //  - The extension has a global side panel specified
      //  - There is currently no global entry registered
      CreateAndRegisterEntry();
    } else if (should_disable_entry) {
      // if the side panel is being disabled for this tab and there exists an
      // entry, deregister it and keep its view.
      DeregisterGlobalEntryAndCacheView();
    }

    return;
  }

  // Ignore changes that don't pertain to this tab id.
  std::optional<int> tab_id =
      IsGlobalCoordinator()
          ? std::nullopt
          : std::make_optional(ExtensionTabUtil::GetTabId(web_contents_));
  if (tab_id != updated_options.tab_id) {
    return;
  }

  // Update the URL if the path was specified.
  GURL previous_url = side_panel_url_;
  if (updated_options.path.has_value()) {
    side_panel_url_ = extension_->GetResourceURL(*updated_options.path);
    if (previous_url != side_panel_url_) {
      global_entry_view_.reset();
    }
  }

  // Deregister the SidePanelEntry if `enabled` is false.
  if (should_disable_entry) {
    DeregisterEntry();
    return;
  }

  bool enabled_for_current_tab =
      !IsGlobalCoordinator() || !IsDisabledForTab(GetCurrentTabId(browser_));
  bool should_create_entry = !entry && should_enable_entry &&
                             enabled_for_current_tab &&
                             !side_panel_url_.is_empty();
  if (should_create_entry) {
    // Create a global entry if the extension has not disabled its side panel
    // for the current tab.
    CreateAndRegisterEntry();
  } else if (entry && previous_url != side_panel_url_) {
    // Handle changes to the side panel's url if an entry exists.
    if (registry_->active_entry() == entry) {
      // If this extension's entry is active, navigate the entry's view to the
      // updated URL.
      NavigateIfNecessary();
    } else {
      // Otherwise, invalidate the cached view and reset the host (since the
      // view will be deleted).
      entry->ClearCachedView();
    }
  }
}

void ExtensionSidePanelCoordinator::OnSidePanelServiceShutdown() {
  scoped_service_observation_.Reset();
}

void ExtensionSidePanelCoordinator::OnViewDestroying() {
  // When the extension's view inside the side panel is destroyed, reset
  // the ExtensionViewHost so it cannot try to notify a view that no longer
  // exists when its event listeners are triggered. Otherwise, a use after free
  // could occur as documented in crbug.com/1403168.
  host_.reset();
  scoped_view_observation_.Reset();
}

void ExtensionSidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  CHECK(IsGlobalCoordinator());
  // Registering/deregistering an entry should only happen if the active tab
  // changes and the extension has specified a global side panel.
  if (!selection.active_tab_changed() ||
      !HasGlobalSidePanel(profile_, *extension_)) {
    return;
  }

  bool disabled_for_old_tab =
      IsDisabledForTab(ExtensionTabUtil::GetTabId(selection.old_contents));
  bool disabled_for_new_tab =
      IsDisabledForTab(ExtensionTabUtil::GetTabId(selection.new_contents));

  if (!disabled_for_old_tab && disabled_for_new_tab) {
    // If we switch to a tab where the extension's global side panel is
    // disabled, deregister the entry but keep its view.
    DeregisterGlobalEntryAndCacheView();
  } else if (disabled_for_old_tab && !disabled_for_new_tab) {
    // If we switch to a tab where the extension's global side panel is enabled,
    // re-register the entry.
    DCHECK(!GetEntry());
    CreateAndRegisterEntry();
  }
}

void ExtensionSidePanelCoordinator::CreateAndRegisterEntry() {
  // The extension icon should be initialized in the constructor, so this should
  // not be null.
  DCHECK(extension_icon_);

  // We use an unretained receiver here: the callback is called only when the
  // SidePanelEntry exists for the extension, and the extension's SidePanelEntry
  // is always deregistered when this class is destroyed, so CreateView can't be
  // called after the destruction of `this`.
  registry_->Register(std::make_unique<SidePanelEntry>(
      GetEntryKey(),
      base::BindRepeating(&ExtensionSidePanelCoordinator::CreateView,
                          base::Unretained(this))));

  auto* coordinator = GetBrowser()->GetFeatures().side_panel_coordinator();
  std::optional<SidePanelCoordinator::UniqueKey> current_key =
      coordinator->current_key();
  bool is_global_entry_showing = current_key && !current_key->tab_handle &&
                                 current_key->key == GetEntryKey();
  // If `entry` is a contextual entry and the global entry with the same key is
  // currently being shown, show the tab-scoped `entry`.
  if (!IsGlobalCoordinator() && is_global_entry_showing) {
    coordinator->Show(
        {tabs::TabInterface::GetFromContents(web_contents_)->GetTabHandle(),
         GetEntryKey()},
        SidePanelUtil::SidePanelOpenTrigger::kExtensionEntryRegistered,
        /*suppress_animations=*/true);
  }
}

std::unique_ptr<views::View> ExtensionSidePanelCoordinator::CreateView() {
  if (global_entry_view_) {
    DCHECK(host_);
    return std::move(global_entry_view_);
  }

  host_ = ExtensionViewHostFactory::CreateSidePanelHost(
      side_panel_url_, browser_, web_contents_);

  // `host_` could be null if `side_panel_url_` is invalid or if the extension
  // is not currently enabled. The latter can happen when the extension has
  // unloaded, and the contextual extension entry is deregistered before the
  // global entry (and so the SidePanelCoordinator tries to show the global
  // one). In this case, return an empty view which should be deleted
  // immediately when the global entry is deregistered.
  if (!host_) {
    DCHECK(!ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
        extension_->id()));
    return std::make_unique<views::WebView>(/*browser_context=*/nullptr);
  }

  // Handle the containing view calling window.close();
  // The base::Unretained() below is safe because this object owns `host_`, so
  // the callback will never fire if `this` is deleted.
  host_->SetCloseHandler(base::BindOnce(
      &ExtensionSidePanelCoordinator::HandleCloseExtensionSidePanel,
      base::Unretained(this)));

  auto extension_view = std::make_unique<ExtensionViewViews>(host_.get());
  extension_view->SetVisible(true);

  scoped_view_observation_.Reset();
  scoped_view_observation_.Observe(extension_view.get());
  return extension_view;
}

void ExtensionSidePanelCoordinator::HandleCloseExtensionSidePanel(
    ExtensionHost* host) {
  DCHECK_EQ(host, host_.get());
  Browser* browser = GetBrowser();
  DCHECK(browser);

  auto* coordinator = browser->GetFeatures().side_panel_coordinator();

  // If the SidePanelEntry for this extension is showing when window.close() is
  // called, close the side panel. Otherwise, clear the entry's cached view.
  SidePanelEntry* entry = GetEntry();
  DCHECK(entry);

  if (coordinator->IsSidePanelEntryShowing(entry->key(), for_tab_)) {
    coordinator->Close();
  } else {
    entry->ClearCachedView();
  }
}

void ExtensionSidePanelCoordinator::NavigateIfNecessary() {
  // Sanity check that this is called when the view exists for this extension's
  // SidePanelEntry.
  DCHECK(host_);

  auto* host_contents = host_->host_contents();
  DCHECK(host_contents);
  if (side_panel_url_ != host_contents->GetLastCommittedURL()) {
    // Since the navigation happens automatically when the URL is changed from
    // an API call, this counts as a top level navigation.
    // TODO(crbug.com/40243760): Investigate if LoadURLWithParams() is needed
    // here, and which params should be used.
    host_contents->GetController().LoadURL(side_panel_url_, content::Referrer(),
                                           ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                           /*extra_headers=*/std::string());
  }
}

void ExtensionSidePanelCoordinator::LoadExtensionIcon() {
  gfx::Image placeholder_icon = ExtensionIconPlaceholder::CreateImage(
      extension_misc::EXTENSION_ICON_BITTY, extension_->name());

  extension_icon_ = std::make_unique<IconImage>(
      profile_, extension_, IconsInfo::GetIcons(extension_),
      extension_misc::EXTENSION_ICON_BITTY, placeholder_icon.AsImageSkia(),
      /*observer=*/nullptr);

  // Triggers actual image loading with all supported scale factors.
  // TODO(crbug.com/40910886): This is a temporary fix since the combobox and
  // its drop down menu currently do not automatically get an image's
  // representation when they are shown. Remove this when the aforementioend
  // crbug has been fixed.
  extension_icon_->image_skia().EnsureRepsForSupportedScales();
}

void ExtensionSidePanelCoordinator::UpdateActionItemIcon() {
  CHECK(IsGlobalCoordinator());
  std::optional<actions::ActionId> extension_action_id =
      actions::ActionIdMap::StringToActionId(GetEntryKey().ToString());
  CHECK(extension_action_id.has_value());
  BrowserActions* browser_actions = browser_->browser_actions();
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      extension_action_id.value(), browser_actions->root_action_item());
  if (action_item) {
    action_item->SetImage(ui::ImageModel::FromImage(extension_icon_->image()));
  }
}

Browser* ExtensionSidePanelCoordinator::GetBrowser() {
  return IsGlobalCoordinator() ? static_cast<Browser*>(browser_)
                               : chrome::FindBrowserWithTab(web_contents_);
}

}  // namespace extensions
