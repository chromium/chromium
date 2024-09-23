// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BROWSER_ACTION_DRAG_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BROWSER_ACTION_DRAG_DATA_H_

#include <stddef.h>

#include <string>

#include "base/memory/stack_allocated.h"
#include "ui/base/dragdrop/os_exchange_data.h"

class Profile;

namespace base {
class Pickle;
}

class BrowserActionDragData {
  STACK_ALLOCATED();

 public:
  BrowserActionDragData();
  BrowserActionDragData(const std::string& id, int index);

  BrowserActionDragData(const BrowserActionDragData&) = delete;
  BrowserActionDragData& operator=(const BrowserActionDragData&) = delete;

  // These mirror the views::View and views::MenuDelegate methods for dropping,
  // and return the appropriate results for being able to drop an extension's
  // BrowserAction view.
  static bool GetDropFormats(std::set<ui::ClipboardFormatType>* format_types);
  static bool AreDropTypesRequired();
  static bool CanDrop(const ui::OSExchangeData& data, const Profile* profile);

  const std::string& id() const { return id_; }

  size_t index() const { return index_; }

  // Returns true if this data is from the specified profile.
  bool IsFromProfile(const Profile* profile) const;

  // Write data, attributed to the specified profile, to the clipboard.
  void Write(Profile* profile, ui::OSExchangeData* data) const;

  // Restores this data from the clipboard, returning true on success.
  bool Read(const ui::OSExchangeData& data);

  // Returns the ClipboardFormatType this class supports (for Browser Actions).
  static const ui::ClipboardFormatType& GetBrowserActionFormatType();

 private:
  void WriteToPickle(Profile* profile, base::Pickle* pickle) const;
  bool ReadFromPickle(base::Pickle* pickle);

  // The profile we originated from.
  void* profile_;

  // The id of the view being dragged.
  std::string id_;

  // The index of the view being dragged.
  size_t index_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BROWSER_ACTION_DRAG_DATA_H_
