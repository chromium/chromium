// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYS_INTERNALS_SYS_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYS_INTERNALS_SYS_INTERNALS_MESSAGE_HANDLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

// The handler class for SysInternals page operations.
class SysInternalsMessageHandler : public content::WebUIMessageHandler {
 public:
  SysInternalsMessageHandler();
  ~SysInternalsMessageHandler() override;

  // content::WebUIMessageHandler methods:
  void RegisterMessages() override;

 private:
  // Handle the Javascript message |getSysInfo|. The message is sent to get
  // system information.
  void HandleGetSysInfo(const base::ListValue* args);

  // The callback function to handle the returning data.
  //
  // |result|: {
  //   const: {
  //     counterMax (Integer: The maximum value of all counters)
  //   }
  //   cpus (Array): [ CpuInfo... ]
  //   memory: {
  //     available (bytes)
  //     total (bytes)
  //     swapFree (bytes)
  //     swapTotal (bytes)
  //     pswpin (counter)
  //     pswpout (counter)
  //   zram: {
  //     origDataSize (bytes)
  //     comprDataSize (bytes)
  //     memUsedTotal (bytes)
  //     numReads (counter)
  //     numWrites (counter)
  //   }
  // }
  //
  // |CpuInfo|: {
  //   kernel (counter)
  //   user (counter)
  //   idle (counter)
  //   total (counter)
  // }
  //
  void ReplySysInfo(base::Value callback_id, base::Value result);

  base::WeakPtrFactory<SysInternalsMessageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SysInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYS_INTERNALS_SYS_INTERNALS_MESSAGE_HANDLER_H_
