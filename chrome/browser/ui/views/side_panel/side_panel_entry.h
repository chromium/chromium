// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view.h"

class SidePanelEntryObserver;

// This class represents an entry inside the side panel. These are owned by
// a SidePanelRegistry (either a per-tab or a per-window registry).
class SidePanelEntry final {
 public:
  using Id = SidePanelEntryId;

  // Container for entry identification related information.
  class Key {
   public:
    explicit Key(Id id);
    Key(Id id, extensions::ExtensionId extension_id);
    Key(const Key& other);
    ~Key();

    Key& operator=(const Key& other);
    bool operator==(const Key& other) const;
    bool operator<(const Key& other) const;

    Id id() const { return id_; }
    absl::optional<extensions::ExtensionId> extension_id() const {
      return extension_id_;
    }

   private:
    Id id_;
    absl::optional<extensions::ExtensionId> extension_id_ = absl::nullopt;
  };

  // If adding a callback to provide a URL to the 'Open in New Tab' button, you
  // must also add a relevant entry in actions.xml because a user action is
  // logged on button click.
  SidePanelEntry(Id id,
                 std::u16string name,
                 ui::ImageModel icon,
                 base::RepeatingCallback<std::unique_ptr<views::View>()>
                     create_content_callback,
                 base::RepeatingCallback<GURL()> open_in_new_tab_url_callback =
                     base::NullCallbackAs<GURL()>());
  // Constructor used for extensions. Extensions don't have 'Open in New Tab'
  // functionality.
  SidePanelEntry(Key key,
                 std::u16string name,
                 ui::ImageModel icon,
                 base::RepeatingCallback<std::unique_ptr<views::View>()>
                     create_content_callback);
  SidePanelEntry(const SidePanelEntry&) = delete;
  SidePanelEntry& operator=(const SidePanelEntry&) = delete;
  ~SidePanelEntry();

  // Creates the content to be shown inside the side panel when this entry is
  // shown.
  std::unique_ptr<views::View> GetContent();
  void CacheView(std::unique_ptr<views::View> view);
  void ClearCachedView();
  views::View* CachedView() {
    return content_view_ ? content_view_.get() : nullptr;
  }

  void ResetIcon(ui::ImageModel icon);

  // Called when the entry has been shown/hidden in the side panel.
  void OnEntryShown();
  void OnEntryHidden();

  const Key& key() const { return key_; }
  const std::u16string& name() const { return name_; }
  const ui::ImageModel& icon() const { return icon_; }

  void AddObserver(SidePanelEntryObserver* observer);
  void RemoveObserver(SidePanelEntryObserver* observer);

  // Gets the 'Open in New Tab' URL. Returns an empty GURL if this function is
  // unavailable for the current side panel entry.
  GURL GetOpenInNewTabURL() const;

  // Returns whether the side panel entry has a defined callback for getting the
  // open new tab button URL.
  bool SupportsNewTabButton();

  base::WeakPtr<SidePanelEntry> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const Key key_;
  const std::u16string name_;
  ui::ImageModel icon_;
  std::unique_ptr<views::View> content_view_;

  base::RepeatingCallback<std::unique_ptr<views::View>()>
      create_content_callback_;

  // If this returns an empty GURL, the 'Open in New Tab' button is hidden.
  base::RepeatingCallback<GURL()> open_in_new_tab_url_callback_;

  base::TimeTicks entry_shown_timestamp_;

  base::ObserverList<SidePanelEntryObserver> observers_;

  base::WeakPtrFactory<SidePanelEntry> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_
