// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_container_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"

ReadAnythingCoordinator::ReadAnythingCoordinator(Browser* browser)
    : BrowserUserData<ReadAnythingCoordinator>(*browser) {
  // Create the model and initialize it with user prefs (if present).
  model_ = std::make_unique<ReadAnythingModel>();
  InitModelWithUserPrefs();

  // Create the controller.
  controller_ = std::make_unique<ReadAnythingController>(model_.get(), browser);
}

void ReadAnythingCoordinator::InitModelWithUserPrefs() {
  Browser* browser = &GetBrowser();
  if (!browser->profile() || !browser->profile()->GetPrefs())
    return;

  std::string prefs_font_name;
  prefs_font_name = browser->profile()->GetPrefs()->GetString(
      prefs::kAccessibilityReadAnythingFontName);

  double prefs_font_scale;
  prefs_font_scale = browser->profile()->GetPrefs()->GetDouble(
      prefs::kAccessibilityReadAnythingFontScale);

  read_anything::mojom::Colors prefs_colors;
  prefs_colors = static_cast<read_anything::mojom::Colors>(
      browser->profile()->GetPrefs()->GetInteger(
          prefs::kAccessibilityReadAnythingColorInfo));

  read_anything::mojom::LineSpacing prefs_line_spacing;
  prefs_line_spacing = static_cast<read_anything::mojom::LineSpacing>(
      browser->profile()->GetPrefs()->GetInteger(
          prefs::kAccessibilityReadAnythingLineSpacing));

  read_anything::mojom::LetterSpacing prefs_letter_spacing;
  prefs_letter_spacing = static_cast<read_anything::mojom::LetterSpacing>(
      browser->profile()->GetPrefs()->GetInteger(
          prefs::kAccessibilityReadAnythingLetterSpacing));

  model_->Init(
      /* font name = */ prefs_font_name,
      /* font scale = */ prefs_font_scale,
      /* colors = */ prefs_colors,
      /* line spacing = */ prefs_line_spacing,
      /* letter spacing = */ prefs_letter_spacing);
}

ReadAnythingCoordinator::~ReadAnythingCoordinator() {
  // Inform observers when |this| is destroyed so they can do their own cleanup.
  for (Observer& obs : observers_) {
    obs.OnCoordinatorDestroyed();
  }

  // Deregister Read Anything from the global side panel registry. This removes
  // Read Anything as a side panel entry observer.
  Browser* browser = &GetBrowser();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return;
  SidePanelRegistry* global_registry =
      SidePanelCoordinator::GetGlobalSidePanelRegistry(browser);
  global_registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
}

void ReadAnythingCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE),
      ui::ImageModel::FromVectorIcon(kReaderModeIcon, ui::kColorIcon),
      base::BindRepeating(&ReadAnythingCoordinator::CreateContainerView,
                          base::Unretained(this)));
  side_panel_entry->AddObserver(this);
  global_registry->Register(std::move(side_panel_entry));
}

ReadAnythingController* ReadAnythingCoordinator::GetController() {
  return controller_.get();
}

ReadAnythingModel* ReadAnythingCoordinator::GetModel() {
  return model_.get();
}

void ReadAnythingCoordinator::AddObserver(
    ReadAnythingCoordinator::Observer* observer) {
  observers_.AddObserver(observer);
}
void ReadAnythingCoordinator::RemoveObserver(
    ReadAnythingCoordinator::Observer* observer) {
  observers_.RemoveObserver(observer);
}
void ReadAnythingCoordinator::AddModelObserver(
    ReadAnythingModel::Observer* observer) {
  DCHECK(model_);
  model_->AddObserver(observer);
}
void ReadAnythingCoordinator::RemoveModelObserver(
    ReadAnythingModel::Observer* observer) {
  DCHECK(model_);
  model_->RemoveObserver(observer);
}

void ReadAnythingCoordinator::OnEntryShown(SidePanelEntry* entry) {
  DCHECK(entry->key().id() == SidePanelEntry::Id::kReadAnything);
  controller_->Activate(true);
}

void ReadAnythingCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  DCHECK(entry->key().id() == SidePanelEntry::Id::kReadAnything);
  controller_->Activate(false);
}

std::unique_ptr<views::View> ReadAnythingCoordinator::CreateContainerView() {
  // Create the views.
  auto toolbar = std::make_unique<ReadAnythingToolbarView>(
      this,
      /* ReadAnythingToolbarView::Delegate* = */ controller_.get(),
      /* ReadAnythingFontCombobox::Delegate* = */ controller_.get());

  Browser* browser = &GetBrowser();
  auto content_web_view = std::make_unique<SidePanelWebUIViewT<ReadAnythingUI>>(
      /* on_show_cb= */ base::RepeatingClosure(),
      /* close_cb= */ base::RepeatingClosure(),
      /* contents_wrapper= */
      std::make_unique<BubbleContentsWrapperT<ReadAnythingUI>>(
          /* webui_url= */ GURL(chrome::kChromeUIReadAnythingSidePanelURL),
          /* browser_context= */ browser->profile(),
          /* task_manager_string_id= */ IDS_READING_MODE_TITLE,
          /* webui_resizes_host= */ false,
          /* esc_closes_ui= */ false));

  // Create the component.
  // Note that a coordinator would normally maintain ownership of these objects,
  // but objects extending {ui/views/view.h} prefer ownership over raw pointers.
  auto container_view = std::make_unique<ReadAnythingContainerView>(
      this, std::move(toolbar), std::move(content_web_view));

  return std::move(container_view);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReadAnythingCoordinator);
