// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_utils.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace extensions {

ExtensionSidePanelCoordinator::ExtensionSidePanelCoordinator(
    Browser* browser,
    const Extension* extension,
    SidePanelRegistry* global_registry)
    : browser_(browser), extension_(extension) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionSidePanelIntegration));
  SidePanelService* service = SidePanelService::Get(browser->profile());
  // `service` can be null for some tests.
  if (service) {
    auto default_options =
        service->GetOptions(*extension, /*tab_id=*/absl::nullopt);
    if (default_options.enabled.has_value() && *default_options.enabled &&
        default_options.path.has_value()) {
      CreateAndRegisterEntry(global_registry,
                             extension->GetResourceURL(*default_options.path));
    }
  }
}

ExtensionSidePanelCoordinator::~ExtensionSidePanelCoordinator() {
  if (auto* global_registry = GetGlobalSidePanelRegistry(browser_)) {
    global_registry->Deregister(
        SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_->id()));
  }
}

void ExtensionSidePanelCoordinator::OnViewDestroying() {
  // When the extension's view inside the side panel is destroyed, reset
  // the ExtensionViewHost so it cannot try to notify a view that no longer
  // exists when its event listeners are triggered. Otherwise, a use after free
  // could occur as documented in crbug.com/1403168.
  host_.reset();
  scoped_view_observation_.Reset();
}

void ExtensionSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry,
    const GURL& side_panel_url) {
  // We use an unretained receiver here: the callback is called only when the
  // SidePanelEntry exists for the extension, and the extension's SidePanelEntry
  // is always deregistered when this class is destroyed, so CreateView can't be
  // called after the destruction of `this`.
  // TODO(crbug.com/1378048): Get the extension's own icon.
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_->id()),
      base::UTF8ToUTF16(extension_->short_name()),
      ui::ImageModel::FromVectorIcon(omnibox::kExtensionAppIcon,
                                     ui::kColorIcon),
      base::BindRepeating(&ExtensionSidePanelCoordinator::CreateView,
                          base::Unretained(this), side_panel_url)));
}

std::unique_ptr<views::View> ExtensionSidePanelCoordinator::CreateView(
    const GURL& side_panel_url) {
  host_ =
      ExtensionViewHostFactory::CreateSidePanelHost(side_panel_url, browser_);

  auto extension_view = std::make_unique<ExtensionViewViews>(host_.get());
  extension_view->SetVisible(true);

  scoped_view_observation_.Observe(extension_view.get());
  return extension_view;
}

}  // namespace extensions
