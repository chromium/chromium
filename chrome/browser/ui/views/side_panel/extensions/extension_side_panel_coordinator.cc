// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace extensions {

ExtensionSidePanelCoordinator::ExtensionSidePanelCoordinator(
    Browser* browser,
    const Extension* extension,
    SidePanelRegistry* global_registry)
    : browser_(browser),
      extension_(extension),
      global_registry_(global_registry) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionSidePanelIntegration));

  // The global registry should always be available for this class.
  DCHECK(global_registry_);

  SidePanelService* service = SidePanelService::Get(browser->profile());
  // `service` can be null for some tests.
  if (service) {
    scoped_service_observation_.Observe(service);
    LoadExtensionIcon();
    auto default_options =
        service->GetOptions(*extension, /*tab_id=*/absl::nullopt);
    if (default_options.enabled.has_value() && *default_options.enabled &&
        default_options.path.has_value()) {
      side_panel_url_ = extension->GetResourceURL(*default_options.path);
      CreateAndRegisterEntry();
    }
  }
}

ExtensionSidePanelCoordinator::~ExtensionSidePanelCoordinator() {
  DeregisterGlobalEntry();
}

content::WebContents*
ExtensionSidePanelCoordinator::GetHostWebContentsForTesting() const {
  DCHECK(host_);
  return host_->host_contents();
}

void ExtensionSidePanelCoordinator::LoadExtensionIconForTesting() {
  LoadExtensionIcon();
}

SidePanelEntry::Key ExtensionSidePanelCoordinator::GetEntryKey() const {
  return SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_->id());
}

void ExtensionSidePanelCoordinator::DeregisterGlobalEntry() {
  global_registry_->Deregister(GetEntryKey());
}

void ExtensionSidePanelCoordinator::OnPanelOptionsChanged(
    const ExtensionId& extension_id,
    const api::side_panel::PanelOptions& updated_options) {
  // Ignore all changes that are not for this extension.
  if (extension_id != extension_->id()) {
    return;
  }

  // TODO(crbug.com/1378048): Handle tab specific side panel options.
  if (updated_options.tab_id.has_value()) {
    return;
  }

  // Update the URL if the path was specified.
  GURL previous_url = side_panel_url_;
  if (updated_options.path.has_value()) {
    side_panel_url_ = extension_->GetResourceURL(*updated_options.path);
  }

  // Deregister the SidePanelEntry if `enabled` is false.
  if (updated_options.enabled.has_value() && !(*updated_options.enabled)) {
    DeregisterGlobalEntry();
    return;
  }

  // If there is no entry for this extension and `enabled` is true, create and
  // register the entry.
  SidePanelEntry::Key key = GetEntryKey();
  auto* entry = global_registry_->GetEntryForKey(key);
  if (!entry) {
    CreateAndRegisterEntry();
  } else if (previous_url != side_panel_url_) {
    if (global_registry_->active_entry().has_value() &&
        (*global_registry_->active_entry())->key() == key) {
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

void ExtensionSidePanelCoordinator::OnExtensionIconImageChanged(
    IconImage* updated_icon) {
  DCHECK_EQ(extension_icon_.get(), updated_icon);

  // If the SidePanelEntry exists for this extension, update its icon.
  // TODO(crbug.com/1378048): Update the icon for all extension entries in
  // contextual registries.
  if (SidePanelEntry* entry = global_registry_->GetEntryForKey(GetEntryKey())) {
    entry->ResetIcon(ui::ImageModel::FromImage(updated_icon->image()));
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
  global_registry_->Register(std::make_unique<SidePanelEntry>(
      GetEntryKey(), base::UTF8ToUTF16(extension_->short_name()),
      ui::ImageModel::FromImage(extension_icon_->image()),
      base::BindRepeating(&ExtensionSidePanelCoordinator::CreateView,
                          base::Unretained(this))));
}

std::unique_ptr<views::View> ExtensionSidePanelCoordinator::CreateView() {
  host_ =
      ExtensionViewHostFactory::CreateSidePanelHost(side_panel_url_, browser_);

  // Handle the containing view calling window.close();
  // The base::Unretained() below is safe because this object owns `host_`, so
  // the callback will never fire if `this` is deleted.
  host_->SetCloseHandler(base::BindOnce(
      &ExtensionSidePanelCoordinator::HandleCloseExtensionSidePanel,
      base::Unretained(this)));

  auto extension_view = std::make_unique<ExtensionViewViews>(host_.get());
  extension_view->SetVisible(true);

  scoped_view_observation_.Observe(extension_view.get());
  return extension_view;
}

void ExtensionSidePanelCoordinator::HandleCloseExtensionSidePanel(
    ExtensionHost* host) {
  DCHECK_EQ(host, host_.get());
  auto* coordinator =
      BrowserView::GetBrowserViewForBrowser(browser_)->side_panel_coordinator();

  // If the SidePanelEntry for this extension is showing when window.close() is
  // called, close the side panel. Otherwise, clear the entry's cached view.
  SidePanelEntry* entry = global_registry_->GetEntryForKey(GetEntryKey());
  DCHECK(entry);

  if (coordinator->IsSidePanelEntryShowing(entry)) {
    coordinator->Close();
  } else {
    entry->ClearCachedView();
  }

  // Closing the panel or removing the view should synchronously result in
  // the extension view being destroyed, which destroys `host_`.
  DCHECK(!host_);
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
    // TODO(crbug.com/1378048): Investigate if LoadURLWithParams() is needed
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
      browser_->profile(), extension_, IconsInfo::GetIcons(extension_),
      extension_misc::EXTENSION_ICON_BITTY, placeholder_icon.AsImageSkia(),
      this);

  // Triggers actual image loading with 1x resources.
  extension_icon_->image_skia().GetRepresentation(1.0f);
}

}  // namespace extensions
