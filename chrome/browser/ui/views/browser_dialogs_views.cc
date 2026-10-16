// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/media/webrtc/select_audio_output_picker.h"
#include "chrome/browser/task_manager/task_manager_metrics_recorder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/select_audio_output/select_audio_output_views.h"
#include "chrome/browser/ui/views/task_manager_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/permissions/chooser_controller.h"
#include "ui/base/class_property.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

// This file provides definitions of desktop browser dialog-creation methods for
// all toolkit-views platforms.

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_window,
                          Profile* profile,
                          const EditDetails& details,
                          Configuration configuration,
                          OnSaveCallback on_save_callback) {
  auto editor = std::make_unique<BookmarkEditorView>(
      profile, details, configuration, std::move(on_save_callback));
  editor->Show(parent_window);
  editor.release();  // BookmarkEditorView is self-deleting
}

void ChromeUsbDevicePermissionsPrompt::ShowDialog() {
  ShowDialogViews();
}

std::unique_ptr<SelectAudioOutputPicker> SelectAudioOutputPicker::Create(
    const content::SelectAudioOutputRequest& request) {
  return std::make_unique<SelectAudioOutputPickerViews>();
}

namespace {

// Waits for `anchor_element_id` to be shown in `element_context` before
// showing the bubble. Owned by `widget` via `kPendingBubbleShowerKey`.
class PendingBubbleShower {
 public:
  PendingBubbleShower(views::Widget* widget,
                      ui::ElementContext element_context,
                      ui::ElementIdentifier anchor_element_id,
                      std::unique_ptr<ui::DialogModel> dialog_model)
      : widget_(widget), dialog_model_(std::move(dialog_model)) {
    subscription_ =
        ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
            anchor_element_id, element_context,
            base::BindRepeating(&PendingBubbleShower::OnElementShown,
                                base::Unretained(this)));
  }

  PendingBubbleShower(const PendingBubbleShower&) = delete;
  PendingBubbleShower& operator=(const PendingBubbleShower&) = delete;
  ~PendingBubbleShower() = default;

 private:
  void OnElementShown(ui::TrackedElement* element);

  const raw_ptr<views::Widget> widget_;
  std::unique_ptr<ui::DialogModel> dialog_model_;
  ui::ElementTracker::Subscription subscription_;
};

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(PendingBubbleShower, kPendingBubbleShowerKey)

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(PendingBubbleShower*)

namespace {

void PendingBubbleShower::OnElementShown(ui::TrackedElement* element) {
  chrome::ShowBubble(element->context(), element->identifier(),
                     std::move(dialog_model_));
  widget_->ClearProperty(kPendingBubbleShowerKey);
}

}  // namespace

namespace chrome {

task_manager::TaskManagerTableModel* ShowTaskManager(
    BrowserWindowInterface* browser,
    task_manager::StartAction start_action) {
  return task_manager::TaskManagerView::Show(browser, start_action);
}

void HideTaskManager() {
  task_manager::TaskManagerView::Hide();
}

views::Widget* ShowBrowserModal(BrowserWindowInterface* browser,
                                std::unique_ptr<ui::DialogModel> dialog_model) {
  return constrained_window::ShowBrowserModal(
      std::move(dialog_model), browser->GetWindow()->GetNativeWindow());
}

// TODO(pbos): Move bubble showing out of this file (like ShowBrowserModal) so
// that this code can be used for showing bubbles outside Browser too.
void ShowBubble(ui::ElementContext element_context,
                ui::ElementIdentifier anchor_element_id,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  ui::TrackedElement* const anchor_element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(
          anchor_element_id, element_context);
  if (!anchor_element) {
    views::Widget* const primary_widget =
        views::ElementTrackerViews::GetInstance()
            ->GetPrimaryWindowWidgetForContext(element_context);
    CHECK(primary_widget, base::NotFatalUntil::M162);
    primary_widget->SetProperty(
        kPendingBubbleShowerKey,
        std::make_unique<PendingBubbleShower>(primary_widget, element_context,
                                              anchor_element_id,
                                              std::move(dialog_model)));
    return;
  }
  // TODO(pbos): Add a version of BubbleBorder::Arrow that infers position
  // automatically based on the anchor's position relative to its widget.
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), views::BubbleAnchor(anchor_element),
      views::BubbleBorder::TOP_RIGHT);
  bubble->SetHighlightedElement(anchor_element_id);
  views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(bubble), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
      ->Show();
}

}  // namespace chrome
