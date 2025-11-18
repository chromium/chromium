// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_VIEWS_H_

#include <stddef.h>

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class LocationBarView;
class OmniboxController;
class OmniboxHeaderView;
class OmniboxPopupViewWebUI;
class OmniboxResultView;
class OmniboxRowGroupedView;
class OmniboxRowView;
class OmniboxViewViews;
struct AutocompleteMatch;

// A view representing the contents of the autocomplete popup.
class OmniboxPopupViewViews : public views::View,
                              public OmniboxPopupView,
                              public OmniboxEditModel::Observer {
  METADATA_HEADER(OmniboxPopupViewViews, views::View)

 public:
  OmniboxPopupViewViews(OmniboxViewViews* omnibox_view,
                        OmniboxController* controller,
                        LocationBarView* location_bar_view);
  OmniboxPopupViewViews(const OmniboxPopupViewViews&) = delete;
  OmniboxPopupViewViews& operator=(const OmniboxPopupViewViews&) = delete;
  ~OmniboxPopupViewViews() override;

  // Returns the icon that should be displayed next to |match|. If the icon is
  // available as a vector icon, it will be |vector_icon_color|.
  gfx::Image GetMatchIcon(const AutocompleteMatch& match,
                          SkColor vector_icon_color) const;

  // Sets the line specified by |index| as selected and, if |index| is
  // different than the previous index, sets the line state to NORMAL.
  virtual void SetSelectedIndex(size_t index);

  // Returns the selected line.
  // Note: This and `SetSelectedIndex` above are used by property
  // metadata and must follow the metadata conventions.
  virtual size_t GetSelectedIndex() const;

  // Returns current popup selection (includes line index).
  virtual OmniboxPopupSelection GetSelection() const;

  void UpdatePopupBounds();

  // OmniboxPopupView:
  void InvalidateLine(size_t line) override;
  void UpdatePopupAppearance() override;
  void ProvideButtonFocusHint(size_t line) override;
  void OnDragCanceled() override;
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const override;
  std::u16string_view GetAccessibleButtonTextForResult(
      size_t line) const override;
  raw_ptr<OmniboxPopupViewWebUI> GetOmniboxPopupViewWebUI() override;

  // views::View:
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds);
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible);
  void OnWidgetDestroying(views::Widget* widget);

  // OmniboxEditModel::Observer:
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection new_selection) override;
  void OnMatchIconUpdated(size_t match_index) override;
  void OnContentsChanged() override;
  void OnKeywordStateChanged(bool is_keyword_selected) override {}

  void FireAXEventsForNewActiveDescendant(View* descendant_view);

 protected:
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupSuggestionGroupHeadersTest,
                           ShowSuggestionGroupHeadersByPageContext);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest, ClickOmnibox);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest, DeleteSuggestion);
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest, SpaceEntersKeywordMode);
  FRIEND_TEST_ALL_PREFIXES(OmniboxRowGroupedViewBrowserTest,
                           AnimationAndGrouping);
  friend class OmniboxPopupViewViewsTest;
  friend class OmniboxRowGroupedViewBrowserTest;
  friend class OmniboxSuggestionButtonRowBrowserTest;
  class PopupWidget;

  // Returns the target popup bounds in screen coordinates based on the bounds
  // of |location_bar_view_|.
  gfx::Rect GetTargetBounds() const;

  // Gets the OmniboxHeaderView for match |i|.
  OmniboxHeaderView* header_view_at(size_t i);

  // Gets the OmniboxResultView for match |i|.
  OmniboxResultView* result_view_at(size_t i);
  const OmniboxResultView* result_view_at(size_t i) const;

  // Returns true if the model has a match at the specified index.
  bool HasMatchAt(size_t index) const;

  // Returns the match at the specified index within the model.
  const AutocompleteMatch& GetMatchAtIndex(size_t index) const;

  // Find the index of the match under the given |point|, specified in window
  // coordinates. Returns OmniboxPopupSelection::kNoMatch if there isn't a match
  // at the specified point.
  size_t GetIndexForPoint(const gfx::Point& point) const;

 private:
  // Classes shouldn't observe multiple objects (to avoid inheriting
  // `base::CheckedObserver` twice). Since `OmniboxPopupViewViews` already
  // observes `OmniboxEditModel`, it needs `WidgetObserverHelper` to observe
  // `Widget`.
  class WidgetObserverHelper : public views::WidgetObserver {
   public:
    explicit WidgetObserverHelper(OmniboxPopupViewViews* popup_view);
    ~WidgetObserverHelper() override;

    // views::WidgetObserver:
    void OnWidgetBoundsChanged(views::Widget* widget,
                               const gfx::Rect& new_bounds) override;
    void OnWidgetVisibilityChanged(views::Widget* widget,
                                   bool visible) override;
    void OnWidgetDestroying(views::Widget* widget) override;

   private:
    raw_ptr<OmniboxPopupViewViews> popup_view_;
  };

  void UpdateAccessibleStates() const;

  void UpdateAccessibleControlIds();

  void UpdateAccessibleActiveDescendantForInvokingView();

  // Updates a row view with the given match data. `previous_row_header` should
  // be supplied with the header value for the previous match in order to detect
  // when the header changes and needs to be displayed. Returns the current
  // row's header value.
  std::u16string UpdateRowView(OmniboxRowView* row_view,
                               const AutocompleteMatch& match,
                               const std::u16string& previous_row_header);

  // Groups the remaining rows of matches starting with the match at
  // `match_start_index` into a group view for a joint animation.
  void UpdateContextualSuggestionsGroup(size_t match_start_index);

  // OmniboxPopupView:
  bool IsOpen() const override;

  // The popup widget that contains this View. Created and closed by `this`;
  // owned and destroyed by the OS. This is a WeakPtr because it's possible for
  // the OS to destroy the window and thus delete this object before `this` is
  // deleted or informed.
  // TODO(crbug.com/40232479): Migrate this to CLIENT_OWNS_WIDGET.
  base::WeakPtr<PopupWidget> widget_;

  // Timestamp for when the current omnibox popup creation started.
  std::optional<base::TimeTicks> popup_create_start_time_;

  // The edit view owned by `location_bar_view_`. May be nullptr in tests.
  const raw_ptr<OmniboxViewViews> omnibox_view_;

  // The location bar view that owns `this`. May be nullptr in tests.
  const raw_ptr<LocationBarView> location_bar_view_;

  // A view that groups together contextual search row views for a joint
  // animation.
  raw_ptr<OmniboxRowGroupedView> contextual_group_view_ = nullptr;

  // The row views that are children of this view or children of subviews of
  // this view like `row_group_view_`.
  std::vector<raw_ptr<OmniboxRowView>> row_views_;

  // Used to observe `views::Widget`. Manual (i.e. unscoped) observation because
  // widgets get created and destroyed during `OmniboxPopupViewViews` lifetime.
  WidgetObserverHelper widget_observer_helper_{this};
  // Used to observe `OmniboxEditModel`.
  base::ScopedObservation<OmniboxEditModel, OmniboxEditModel::Observer>
      edit_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_VIEWS_H_
