// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_ID_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_ID_H_

#include "base/token.h"

class TabGroupId {
 public:
  static TabGroupId GenerateNew();

  // This should only called with |token| returned from a previous |token()|
  // call on a valid TabGroupId.
  static TabGroupId FromRawToken(base::Token token);

  TabGroupId(const TabGroupId& other);

  TabGroupId& operator=(const TabGroupId& other);

  bool operator==(const TabGroupId& other) const;
  bool operator!=(const TabGroupId& other) const;
  bool operator<(const TabGroupId& other) const;

  base::Token token() const { return token_; }

  std::string ToString() const;

 private:
  explicit TabGroupId(base::Token token);

  base::Token token_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_ID_H_
