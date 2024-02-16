// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CROSTINI_APP_DISPLAY_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CROSTINI_APP_DISPLAY_H_

#include <deque>
#include <map>
#include <string>

// Manages mapping from a Crostini app ID to a display ID.
class CrostiniAppDisplay {
 public:
  CrostiniAppDisplay();

  CrostiniAppDisplay(const CrostiniAppDisplay&) = delete;
  CrostiniAppDisplay& operator=(const CrostiniAppDisplay&) = delete;

  ~CrostiniAppDisplay();

  // Register that |app_id| app should be shown in |display_id| monitor.
  void Register(const std::string& app_id, int64_t display_id);
  // Returns the display ID that the |app_id| app should be shown in.
  int64_t GetDisplayIdForAppId(const std::string& app_id);

 private:
  // Since there is no message when an app quits, maintain a maximum number so
  // that older ones are deleted.
  const unsigned kMaxAppIdSize = 32;

  std::map<std::string, int64_t> app_id_to_display_id_;
  std::deque<std::string> app_ids_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CROSTINI_APP_DISPLAY_H_
