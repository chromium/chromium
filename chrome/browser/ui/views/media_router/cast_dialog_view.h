// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_access_code_cast_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/md_text_button_with_down_arrow.h"
#include "ui/views/controls/menu/menu_runner.h"

class Profile;

namespace media_router {

class CastDialogSinkView;
enum class MediaRouterDialogActivationLocation;
struct UIMediaSink;

// View component of the Cast dialog that allows users to start and stop Casting
// to devices. The list of devices used to populate the dialog is supplied by
// CastDialogModel.
class CastDialogView : public views::BubbleDialogDelegateView,
                       public CastDialogController::Observer,
                       public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(CastDialogView, views::BubbleDialogDelegateView)

 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDialogModelUpdated(CastDialogView* dialog_view) = 0;
    virtual void OnDialogWillClose(CastDialogView* dialog_view) = 0;
  };

  enum SourceType { kTab, kDesktop };

  CastDialogView(views::View* anchor_view,
                 views::BubbleBorder::Arrow anchor_position,
                 CastDialogController* controller,
                 Profile* profile,
                 const base::Time& start_time,
                 MediaRouterDialogActivationLocation activation_location,
                 actions::ActionItem* action_item = nullptr);
  ~CastDialogView() override;
  CastDialogView(const CastDialogView&) = delete;
  CastDialogView& operator=(const CastDialogView&) = delete;

  // views::WidgetDelegate:
  std::u16string GetWindowTitle() const override;

  // CastDialogController::Observer:
  void OnModelUpdated(const CastDialogModel& model) override;
  void OnControllerDestroying() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // If the dialog loses focus during a test and closes, the test can
  // fail unexpectedly. This method prevents that by keeping the dialog from
  // closing on blur.
  void KeepShownForTesting();

  // Called by tests.
  const std::vector<raw_ptr<CastDialogSinkView, DanglingUntriaged>>&
  sink_views_for_test() const {
    return sink_views_;
  }
  views::ScrollView* scroll_view_for_test() { return scroll_view_; }
  views::View* no_sinks_view_for_test() { return no_sinks_view_; }
  views::View* permission_rejected_view_for_test() {
    return permission_rejected_view_;
  }
  views::Button* sources_button_for_test() { return sources_button_; }
  HoverButton* access_code_cast_button_for_test() {
    return access_code_cast_button_;
  }
  ui::SimpleMenuModel* sources_menu_model_for_test() {
    return sources_menu_model_.get();
  }
  views::MenuRunner* sources_menu_runner_for_test() {
    return sources_menu_runner_.get();
  }

 private:
  // TODO(crbug.com/1346127): Remove friend classes.
  friend class CastDialogViewTest;
  friend class MediaRouterCastUiForTest;
  FRIEND_TEST_ALL_PREFIXES(CastDialogViewTest, DisableUnsupportedSinks);
  FRIEND_TEST_ALL_PREFIXES(CastDialogViewTest, ShowAndHideDialog);
  FRIEND_TEST_ALL_PREFIXES(CastDialogViewTest, ShowSourcesMenu);

  // views::BubbleDialogDelegateView:
  void Init() override;
  void WindowClosing() override;

  void ShowAccessCodeCastDialog();
  void MaybeShowAccessCodeCastButton();

  void ShowNoSinksView();
  void ShowPermissionRejectedView();
  void ShowScrollView();
  void ResetViews();

  // Applies the stored scroll state.
  void RestoreSinkListState();

  // Populates the scroll view containing sinks using the data in |model|.
  void PopulateScrollView(const std::vector<UIMediaSink>& sinks);

  void InitializeSourcesButton();

  // Shows the sources menu that allows the user to choose a source to cast.
  void ShowSourcesMenu();

  // Stores |source| as the source to be used when user selects a sink to start
  // casting, and updates the UI to reflect the selection.
  void SelectSource(SourceType source);

  void SinkPressed(size_t index);
  void IssuePressed(size_t index);
  void StopPressed(size_t index);
  void FreezePressed(size_t index);

  void MaybeSizeToContents();

  // Returns the cast mode that is selected in the sources menu and supported by
  // |sink|. Returns nullopt if no such cast mode exists.
  std::optional<MediaCastMode> GetCastModeToUse(const UIMediaSink& sink) const;

  // Disables sink buttons for sinks that do not support the currently selected
  // source.
  void DisableUnsupportedSinks();

  // Posts a delayed task to record the number of sinks shown with the metrics
  // recorder.
  void RecordSinkCountWithDelay();

  // Records the number of sinks shown with the metrics recorder.
  void RecordSinkCount();

  // Returns true iff feature is turned on and the access code casting policy
  // has been enabled for this user.
  bool IsAccessCodeCastingEnabled() const;

  // Title shown at the top of the dialog.
  std::u16string dialog_title_;

  // The source selected in the sources menu. This defaults to "tab"
  // (presentation or tab mirroring). "Tab" is represented by a single item in
  // the sources menu.
  SourceType selected_source_ = SourceType::kTab;

  // Contains references to sink views in the order they appear.
  std::vector<raw_ptr<CastDialogSinkView, DanglingUntriaged>> sink_views_;

  raw_ptr<CastDialogController> controller_;

  // ScrollView containing the list of sink buttons.
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;

  // View shown while there are no sinks.
  raw_ptr<views::View> no_sinks_view_ = nullptr;
  raw_ptr<views::View> permission_rejected_view_ = nullptr;

  const raw_ptr<Profile> profile_;

  // How much |scroll_view_| is scrolled downwards in pixels. Whenever the sink
  // list is updated the scroll position gets reset, so we must manually restore
  // it to this value.
  int scroll_position_ = 0;

  // The access code cast button allows the user to add a cast device through
  // the chrome://access-code-cast dialog.
  raw_ptr<CastDialogAccessCodeCastButton> access_code_cast_button_ = nullptr;

  // The sources menu allows the user to choose a source to cast.
  raw_ptr<views::MdTextButtonWithDownArrow> sources_button_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> sources_menu_model_;
  std::unique_ptr<views::MenuRunner> sources_menu_runner_;

  // Records UMA metrics for the dialog's behavior.
  CastDialogMetrics metrics_;

  // The action item tied to this dialog and its anchor view.
  // Used to handle bubble re-opening issues with Pinned Toolbar Buttons.
  const raw_ptr<actions::ActionItem> action_item_ = nullptr;

  // The sink that the user has selected to cast to. If the user is using
  // multiple sinks at the same time, the last activated sink is used.
  std::optional<size_t> selected_sink_index_;

  base::ObserverList<Observer> observers_;

  // When this is set to true, the dialog does not close on blur.
  bool keep_shown_for_testing_ = false;

  base::WeakPtrFactory<CastDialogView> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_VIEW_H_
