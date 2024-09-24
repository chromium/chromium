// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_row_view_info.h"
#include "chrome/browser/ui/download/download_item_mode.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "components/download/public/common/download_item.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ui {
class Event;
}

namespace views {
class ImageView;
class InputEventActivationProtector;
class Label;
class MdTextButton;
class ImageButton;
class ProgressBar;
class FlexLayoutView;
class InkDropContainerView;
}  // namespace views

class DownloadShelfContextMenuView;
class DownloadBubbleUIController;

class DownloadBubbleRowView : public views::View,
                              public views::ContextMenuController,
                              public views::FocusChangeListener,
                              public views::ViewTargeterDelegate,
                              public DownloadBubbleRowViewInfoObserver {
  METADATA_HEADER(DownloadBubbleRowView, views::View)

 public:
  explicit DownloadBubbleRowView(
      const DownloadBubbleRowViewInfo& info,
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      base::WeakPtr<Browser> browser,
      int fixed_width,
      bool is_in_partial_view = false);
  DownloadBubbleRowView(const DownloadBubbleRowView&) = delete;
  DownloadBubbleRowView& operator=(const DownloadBubbleRowView&) = delete;
  ~DownloadBubbleRowView() override;

  // Overrides views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Layout(PassKey) override;
  Views GetChildrenInZOrder() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // Overrides views::FocusChangeListener
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override {}

  // Update the row and its elements for hover and focus events.
  void UpdateRowForHover(bool hovered);
  void UpdateRowForFocus(bool visible, bool request_focus_on_last_quick_action);

  // Overrides views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Overrides ui::AcceleratorTarget
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // Returns the transparent button that is activated when the row is clicked.
  views::Button* transparent_button() { return transparent_button_; }

  const std::u16string& GetSecondaryLabelTextForTesting();

  DownloadUIModel* model() { return info_->model(); }
  const DownloadBubbleRowViewInfo& info() const { return *info_; }

  void SimulateMainButtonClickForTesting(const ui::Event& event);
  bool IsQuickActionButtonVisibleForTesting(DownloadCommands::Command command);
  views::ImageButton* GetQuickActionButtonForTesting(
      DownloadCommands::Command command);
  void SetInputProtectorForTesting(
      std::unique_ptr<views::InputEventActivationProtector> input_protector);

  // views::ViewTargeterDelegate
  View* TargetForRect(View* root, const gfx::Rect& rect) override;

 protected:
  // Overrides ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  void AddMainPageButton(DownloadCommands::Command command,
                         const std::u16string& button_string);
  views::ImageButton* AddQuickAction(DownloadCommands::Command command);
  views::ImageButton* GetActionButtonForCommand(
      DownloadCommands::Command command);
  std::u16string GetAccessibleNameForQuickAction(
      DownloadCommands::Command command);
  std::u16string GetAccessibleNameForMainPageButton(
      DownloadCommands::Command command);

  // Update the DownloadBubbleRowView's members.
  void UpdateRow(bool initial_setup);

  void UpdateStatusText();
  void UpdateButtons();
  void UpdateProgressBar();
  void UpdateLabels();
  void UpdateDeepScanNotice();
  void RecordMetricsOnUpdate();
  void RecordDownloadDisplayed();

  // Load the appropriate |file_icon_| from the IconManager, or a default icon.
  // Returns whether we were able to synchronously set |icon_| to an appropriate
  // icon for the file path.
  bool StartLoadFileIcon();
  // Callback invoked when the IconManager's lookup returns.
  void OnFileIconLoaded(gfx::Image icon);
  // Sets |icon_| to the image in |file_icon_|.
  void SetFileIconAsIcon(bool is_default_icon);

  // Set the |icon_|, which may be an override (warning or incognito icon),
  // default icon, or loaded from the cache or from IconManager::LoadIcon.
  void SetIcon();

  // Sets |icon_| to |icon|, regardless of what kind of icon it is.
  void SetIconFromImage(gfx::Image icon);
  void SetIconFromImageModel(const ui::ImageModel& icon);

  // Called when the transparent button (covering the whole row) is pressed.
  void OnMainButtonPressed(const ui::Event& event);
  // Called when the button on the side of the row (the "main page button") or a
  // quick action button is pressed.
  void OnActionButtonPressed(DownloadCommands::Command command,
                             const ui::Event& event);

  // Registers/unregisters copy accelerator for copy/paste support.
  void RegisterAccelerators(views::FocusManager* focus_manager);
  void UnregisterAccelerators(views::FocusManager* focus_manager);

  // DownloadBubbleRowViewInfoObserver implementation:
  void OnInfoChanged() override;

  // The icon for the file. We get platform-specific file type icons from
  // IconLoader (see below).
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::ImageView> subpage_icon_ = nullptr;
  raw_ptr<views::FlexLayoutView> subpage_icon_holder_ = nullptr;
  // The icon for the filetype, fetched from the platform-specific IconLoader.
  // This can differ from the image in |icon_| if |icon_| is not the file type
  // icon, e.g. if it is the incognito icon or a warning icon. We cache it here
  // in case |icon_| is different, because it is used when drag-and-dropping.
  // If the IconLoader does not return a file icon, this stores a default icon.
  gfx::Image file_icon_;

  // The primary label.
  raw_ptr<views::Label> primary_label_ = nullptr;

  // The secondary label.
  raw_ptr<views::Label> secondary_label_ = nullptr;

  // Buttons on the main page.
  base::flat_map<DownloadCommands::Command, raw_ptr<views::MdTextButton>>
      main_page_buttons_;

  // Quick Actions on the main page.
  raw_ptr<views::ImageButton> resume_action_ = nullptr;
  raw_ptr<views::ImageButton> pause_action_ = nullptr;
  raw_ptr<views::ImageButton> show_in_folder_action_ = nullptr;
  raw_ptr<views::ImageButton> cancel_action_ = nullptr;
  raw_ptr<views::ImageButton> open_when_complete_action_ = nullptr;

  // Holder for the main button.
  raw_ptr<views::FlexLayoutView> main_button_holder_ = nullptr;
  // Holder for the quick actions.
  raw_ptr<views::FlexLayoutView> quick_action_holder_ = nullptr;

  // The progress bar for in-progress downloads.
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<views::FlexLayoutView> progress_bar_holder_ = nullptr;

  // Device scale factor, used to load icons.
  float current_scale_ = 1.0f;

  // The model controlling this object's state.
  raw_ref<const DownloadBubbleRowViewInfo> info_;

  // Reuse the download shelf context menu in the bubble.
  std::unique_ptr<DownloadShelfContextMenuView> context_menu_;

  // Controller for keeping track of downloads.
  base::WeakPtr<DownloadBubbleUIController> bubble_controller_ = nullptr;

  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;

  base::WeakPtr<Browser> browser_ = nullptr;

  // The last override icon, e.g. an incognito or warning icon. If this is
  // null, we should either use the filetype icon or a default icon.
  raw_ptr<const gfx::VectorIcon> last_overridden_icon_ = nullptr;
  // Whether the currently set |icon_| is the default icon.
  bool has_default_icon_ = false;

  // Button for transparent button click, inkdrop animations and drag and drop
  // events.
  raw_ptr<views::Button> transparent_button_ = nullptr;

  raw_ptr<views::InkDropContainerView> inkdrop_container_;

#if !BUILDFLAG(IS_CHROMEOS)
  raw_ptr<views::View> deep_scan_notice_;
#endif

  // Drag and drop:
  // Whether we are dragging the download bubble row.
  bool dragging_ = false;
  // Position that a possible drag started at.
  std::optional<gfx::Point> drag_start_point_;

  // Whether the download's completion has already been logged. This is used to
  // avoid inaccurate repeated logging.
  bool has_download_completion_been_logged_ = false;

  // A timer for updating the status text string.
  base::RepeatingTimer update_status_text_timer_;

  // Tracks tasks requesting file icons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Mitigates the risk of clickjacking by enforcing a delay in click input.
  std::unique_ptr<views::InputEventActivationProtector> input_protector_;

  // Used for metrics to study clickjacking potential.
  const base::Time shown_time_;
  // False in tests.
  const bool is_in_partial_view_ = false;

  // TODO(crbug.com/40233803): The size constraint is not passed down from the
  // views tree in the first round of layout, so setting a fixed width to bound
  // the view. This is assuming that the row view is loaded inside a bubble. It
  // will break if the row view is loaded inside a different parent view.
  const int fixed_width_;

  base::WeakPtrFactory<DownloadBubbleRowView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
