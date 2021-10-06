// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_
#define COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_

#include <memory>
#include <string>

#include "base/component_export.h"

namespace app_restore {

class RestoreData;
struct WindowInfo;

// A class which acts as a proxy to fetch data from a delegate class located in
// chrome/browser.
// TODO(sammiequon): Revisit after M93 and move this class to a more suitable
// location, outside of components/app_restore as it is part of a different
// feature.
class COMPONENT_EXPORT(APP_RESTORE) DeskTemplateReadHandler {
 public:
  class Delegate {
   public:
    // Gets the window information for `restore_window_id`.
    virtual std::unique_ptr<WindowInfo> GetWindowInfo(
        int restore_window_id) = 0;

    // Fetches the restore id for the window from RestoreData for the given
    // `app_id`. `app_id` should be a Chrome app id.
    virtual int32_t FetchRestoreWindowId(const std::string& app_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  static DeskTemplateReadHandler* GetInstance();

  DeskTemplateReadHandler();
  DeskTemplateReadHandler(const DeskTemplateReadHandler&) = delete;
  DeskTemplateReadHandler& operator=(const DeskTemplateReadHandler&) = delete;
  ~DeskTemplateReadHandler();

  void SetDelegate(Delegate* delegate);

  // Gets the window information for `restore_window_id`.
  std::unique_ptr<WindowInfo> GetWindowInfo(int restore_window_id);

  // Fetches the restore id for the window from RestoreData for the given
  // `app_id`. `app_id` should be a Chrome app id.
  int32_t FetchRestoreWindowId(const std::string& app_id);

 private:
  Delegate* delegate_ = nullptr;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_
