// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class TabbedPane;
class MdTextButton;
}  // namespace views

class DesktopMediaPickerViews;

// Dialog view used for DesktopMediaPickerViews.
//
// TODO(crbug.com/987001): Consider renaming this class.
class DesktopMediaPickerDialogView : public views::DialogDelegateView,
                                     public views::TabbedPaneListener {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kDesktopMediaPickerDialogViewIdentifier);
  // Used for UMA. Visible to this class's .cc file, but opaque beyond.
  enum class DialogType : int;

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
  void OnDelegatedSourceListDismissed();
  void OnCanReselectChanged(const DesktopMediaListController* controller);

  // Relevant for UMA. (E.g. for DesktopMediaPickerViews to report
  // when the dialog gets dismissed.)
  DialogType GetDialogType() const;

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
  void OnWidgetInitialized() override;

 private:
  friend class DesktopMediaPickerViewsTestApi;

  struct DisplaySurfaceCategory {
    DisplaySurfaceCategory(
        DesktopMediaList::Type type,
        std::unique_ptr<DesktopMediaListController> controller,
        bool audio_offered,
        bool audio_checked,
        bool supports_reselect_button);

    DisplaySurfaceCategory(DisplaySurfaceCategory&& other);

    ~DisplaySurfaceCategory();

    DesktopMediaList::Type type;
    std::unique_ptr<DesktopMediaListController> controller;
    bool audio_offered;  // Whether the audio-checkbox should be visible.
    bool audio_checked;  // Whether the audio-checkbox is checked.
    // Whether to show a button to allow re-selecting a choice within this
    // category. Primarily used if there is a separate selection surface that we
    // may need to re-open.
    bool supports_reselect_button;
  };

  static bool AudioSupported(DesktopMediaList::Type type);

  void ConfigureUIForNewPane(int index);
  void StoreAudioCheckboxState();
  void RemoveCurrentPaneUI();
  void MaybeCreateReselectButtonForPane(const DisplaySurfaceCategory& category);
  void MaybeCreateAudioCheckboxForPane(const DisplaySurfaceCategory& category);
  void MaybeSetAudioCheckboxMaxSize();

  void OnSourceTypeSwitched(int index);

  int GetSelectedTabIndex() const;

  const DesktopMediaListController* GetSelectedController() const;
  DesktopMediaListController* GetSelectedController();

  DesktopMediaList::Type GetSelectedSourceListType() const;

  const raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  const bool is_get_display_media_call_;
  const bool audio_requested_;
  const bool suppress_local_audio_playback_;  // Effective only if audio shared.
  const content::GlobalRenderFrameHostId capturer_global_id_;

  raw_ptr<DesktopMediaPickerViews> parent_;

  raw_ptr<views::Label> description_label_ = nullptr;

  raw_ptr<views::Checkbox> audio_share_checkbox_ = nullptr;

  raw_ptr<views::MdTextButton> reselect_button_ = nullptr;

  raw_ptr<views::TabbedPane> tabbed_pane_ = nullptr;
  std::vector<DisplaySurfaceCategory> categories_;
  int previously_selected_category_ = 0;

  DialogType dialog_type_;

  absl::optional<content::DesktopMediaID> accepted_source_;
};

// Implementation of DesktopMediaPicker for Views.
//
// TODO(crbug.com/987001): Rename this class.  Consider merging with
// DesktopMediaPickerController and naming the merged class just
// DesktopMediaPicker.
class DesktopMediaPickerViews : public DesktopMediaPicker {
 public:
#if BUILDFLAG(IS_WIN) || defined(USE_CRAS)
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

  bool is_get_display_media_call_ = false;

  // The |dialog_| is owned by the corresponding views::Widget instance.
  // When DesktopMediaPickerViews is destroyed the |dialog_| is destroyed
  // asynchronously by closing the widget.
  raw_ptr<DesktopMediaPickerDialogView> dialog_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
