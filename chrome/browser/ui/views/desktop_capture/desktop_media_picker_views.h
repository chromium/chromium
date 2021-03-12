// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_

#include "base/callback_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class TabbedPane;
}  // namespace views

class DesktopMediaPickerViews;

// Dialog view used for DesktopMediaPickerViews.
//
// TODO(crbug.com/987001): Consider renaming this class.
class DesktopMediaPickerDialogView : public views::DialogDelegateView,
                                     public views::TabbedPaneListener {
 public:
  // Used for UMA. Visible to this class's .cc file, but opaque beyond.
  enum class DialogSource : int;

  METADATA_HEADER(DesktopMediaPickerDialogView);
  DesktopMediaPickerDialogView(
      const DesktopMediaPicker::Params& params,
      DesktopMediaPickerViews* parent,
      std::vector<std::unique_ptr<DesktopMediaList>> source_lists);
  DesktopMediaPickerDialogView(const DesktopMediaPickerDialogView&) = delete;
  DesktopMediaPickerDialogView& operator=(const DesktopMediaPickerDialogView&) =
      delete;
  ~DesktopMediaPickerDialogView() override;

  // Called by parent (DesktopMediaPickerViews) when it's destroyed.
  void DetachParent();

  // Called by DesktopMediaListController.
  void OnSelectionChanged();
  void AcceptSource();
  void AcceptSpecificSource(content::DesktopMediaID source);
  void Reject();
  void OnSourceListLayoutChanged();

  // Relevant for UMA. (E.g. for DesktopMediaPickerViews to report
  // when the dialog gets dismissed.)
  DialogSource GetDialogSource() const;

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  std::u16string GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  bool Accept() override;
  bool Cancel() override;
  bool ShouldShowCloseButton() const override;
  void DeleteDelegate() override;

 private:
  friend class DesktopMediaPickerViewsTestApi;

  void OnSourceTypeSwitched(int index);

  int GetSelectedTabIndex() const;

  const DesktopMediaListController* GetSelectedController() const;
  DesktopMediaListController* GetSelectedController();

  DesktopMediaList::Type GetSelectedSourceListType() const;

  content::WebContents* const web_contents_;

  DesktopMediaPickerViews* parent_;

  views::Label* description_label_ = nullptr;

  views::Checkbox* presenter_tools_checkbox_ = nullptr;

  base::CallbackListSubscription presenter_tools_checked_subscription_;

  views::Checkbox* audio_share_checkbox_ = nullptr;

  // Contains |presenter_tools_checkbox_| and |audio_share_checkbox_| if
  // present.
  views::View* extra_views_container_ = nullptr;

  views::TabbedPane* tabbed_pane_ = nullptr;
  std::vector<std::unique_ptr<DesktopMediaListController>> list_controllers_;
  std::vector<DesktopMediaList::Type> source_types_;

  DialogSource dialog_source_;

  base::Optional<content::DesktopMediaID> accepted_source_;
};

// Implementation of DesktopMediaPicker for Views.
//
// TODO(crbug.com/987001): Rename this class.  Consider merging with
// DesktopMediaPickerController and naming the merged class just
// DesktopMediaPicker.
class DesktopMediaPickerViews : public DesktopMediaPicker {
 public:
#if defined(OS_WIN) || defined(USE_CRAS)
  static constexpr bool kScreenAudioShareSupportedOnPlatform = true;
#else
  static constexpr bool kScreenAudioShareSupportedOnPlatform = false;
#endif

  DesktopMediaPickerViews();
  DesktopMediaPickerViews(const DesktopMediaPickerViews&) = delete;
  DesktopMediaPickerViews& operator=(const DesktopMediaPickerViews&) = delete;
  ~DesktopMediaPickerViews() override;

  void NotifyDialogResult(content::DesktopMediaID source);

  // DesktopMediaPicker:
  void Show(const DesktopMediaPicker::Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            DoneCallback done_callback) override;

  DesktopMediaPickerDialogView* GetDialogViewForTesting() const {
    return dialog_;
  }

 private:
  friend class DesktopMediaPickerViewsTestApi;

  DoneCallback callback_;

  // The |dialog_| is owned by the corresponding views::Widget instance.
  // When DesktopMediaPickerViews is destroyed the |dialog_| is destroyed
  // asynchronously by closing the widget.
  DesktopMediaPickerDialogView* dialog_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
