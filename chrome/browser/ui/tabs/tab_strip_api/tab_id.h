// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ID_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ID_H_

#include <string>
#include <string_view>

namespace tabs_api {

// A discrete object representing an id for a tab resource.
// Clients should never construct their own id instance and should only
// use the IDs returned by the tab service.
class TabId {
 public:
  enum class Type {
    kInvalid,
    // kContent ids are mapped to TabHandle::Handle, which represents an
    // int32_t number.
    kContent,
    kCollection,
  };

  TabId() : TabId(Type::kInvalid, "") {}
  TabId(enum Type type, std::string_view id) : type_(type), id_(id) {}
  ~TabId() = default;

  std::string_view Id() const { return id_; }

  Type Type() const { return type_; }

  // Two tab ids are equal iff they represent the same underlying resource
  // (denoted by the type) and they have the same id.
  friend bool operator==(const TabId& a, const TabId& b);

 private:
  enum Type type_;
  std::string id_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_ID_H_
