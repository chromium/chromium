// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_ACTION_HANDLER_H_
#define CHROME_UPDATER_ACTION_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"

namespace update_client {
class ActionHandler;
}

namespace updater {

scoped_refptr<update_client::ActionHandler> MakeActionHandler();

#if !defined(OS_WIN)
// TODO(sorin): implement the action handler for other platforms.
// http://crbug.com/1030863
inline scoped_refptr<update_client::ActionHandler> MakeActionHandler() {
  return nullptr;
}
#endif

}  // namespace updater

#endif  // CHROME_UPDATER_ACTION_HANDLER_H_
