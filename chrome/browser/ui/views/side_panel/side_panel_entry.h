// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "extensions/common/extension_id.h"
#include "ui/base/class_property.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/view.h"

class SidePanelEntryScope;
class SidePanelEntryObserver;
enum class SidePanelEntryHideReason;

// This class represents an entry inside the side panel. These are owned by
// a SidePanelRegistry (either a per-tab or a per-window registry).
class SidePanelEntry final : public ui::PropertyHandler {
 public:
  // The default and minimum acceptable side panel content width.
  static constexpr int kSidePanelDefaultContentWidth = 360;
  using CreateContentCallback =
      base::RepeatingCallback<std::unique_ptr<views::View>(
          SidePanelEntryScope&)>;
  using Id = SidePanelEntryId;
  using Key = SidePanelEntryKey;

  // If adding a callback to provide a URL to the 'Open in New Tab' button, you
  // must also add a relevant entry in actions.xml because a user action is
  // logged on button click.
  SidePanelEntry(Key key,
                 CreateContentCallback create_content_callback,
                 base::RepeatingCallback<GURL()> open_in_new_tab_url_callback,
                 base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()>
                     more_info_callback,
                 int default_content_width);

  // This constructor is primarily used for extensions.Extensions don't have
  // `Open in New Tab` functionality. Other side panels can use this if nothing
  // custom is needed (we call the other constructor passing
  // base::NullCallback()).
  SidePanelEntry(Key key,
                 CreateContentCallback create_content_callback,
                 int default_content_width);
  SidePanelEntry(const SidePanelEntry&) = delete;
  SidePanelEntry& operator=(const SidePanelEntry&) = delete;
  ~SidePanelEntry() override;

  // Creates the content to be shown inside the side panel when this entry is
  // shown.
  std::unique_ptr<views::View> GetContent();
  void CacheView(std::unique_ptr<views::View> view);
  void ClearCachedView();
  views::View* CachedView() {
    return content_view_ ? content_view_.get() : nullptr;
  }

  // Called when the entry has been shown/hidden in the side panel.
  void OnEntryShown();
  void OnEntryWillHide(SidePanelEntryHideReason reason);
  void OnEntryHidden();

  const Key& key() const { return key_; }

  void AddObserver(SidePanelEntryObserver* observer);
  void RemoveObserver(SidePanelEntryObserver* observer);

  // Gets the 'Open in New Tab' URL. Returns an empty GURL if this function is
  // unavailable for the current side panel entry.
  GURL GetOpenInNewTabURL() const;

  // Gets the menu model for the more info menu if the current side panel entry
  // has one, otherwise null.
  std::unique_ptr<ui::MenuModel> GetMoreInfoMenuModel() const;

  // Returns whether the side panel entry has a defined callback for getting the
  // open new tab button URL.
  bool SupportsNewTabButton();

  // Returns whether the side panel entry has a defined callback for the more
  // info button.
  bool SupportsMoreInfoButton();

  // Resets the `entry_show_triggered_timestamp_` so we don't track metrics
  // incorrectly.
  void ResetLoadTimestamp();

  void set_scope(SidePanelEntryScope* scope) { scope_ = scope; }

  base::WeakPtr<SidePanelEntry> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Gets the default content width for this entry, if one is specified.
  int GetDefaultContentWidth() const;

  // Allows tests to override the default width for an existing entry.
  void SetDefaultContentWidthForTesting(int width) {
    default_content_width_ = width;
  }

 private:
  const Key key_;
  std::unique_ptr<views::View> content_view_;

  // Scope of this entry, will outlive the entry and its content.
  raw_ptr<SidePanelEntryScope> scope_ = nullptr;

  CreateContentCallback create_content_callback_;

  // If this returns an empty GURL, the 'Open in New Tab' button is hidden.
  base::RepeatingCallback<GURL()> open_in_new_tab_url_callback_;

  // If this returns null, the more info button is hidden.
  base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()> more_info_callback_;

  // Timestamp of when the side panel was triggered to be shown.
  base::TimeTicks entry_show_triggered_timestamp_;

  base::TimeTicks entry_shown_timestamp_;

  base::ObserverList<SidePanelEntryObserver> observers_;

  // When specified sets the default starting width for this entry. However, if
  // the user manually changes the size of the side panel that preference is
  // used instead (prefs::kSidePanelIdToWidth). If nothing is specified, then
  // the default minimum content width of the side panel is used.
  int default_content_width_ = kSidePanelDefaultContentWidth;

  base::WeakPtrFactory<SidePanelEntry> weak_factory_{this};
};

extern const ui::ClassProperty<bool>* const
    kShouldShowTitleInSidePanelHeaderKey;

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_
