// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_QUICK_PAIR_QUICK_PAIR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_QUICK_PAIR_QUICK_PAIR_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/components/quick_pair/common/log_buffer.h"
#include "chromeos/components/quick_pair/common/logging.h"
#include "content/public/browser/web_ui_message_handler.h"

// WebUIMessageHandler for the Quick Pair debug page at
// chrome://nearby-internals
class QuickPairHandler : public content::WebUIMessageHandler,
                         public chromeos::quick_pair::LogBuffer::Observer {
 public:
  QuickPairHandler();
  QuickPairHandler(const QuickPairHandler&) = delete;
  QuickPairHandler& operator=(const QuickPairHandler&) = delete;
  ~QuickPairHandler() override;

  // content::WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // LogBuffer::Observer
  void OnLogMessageAdded(
      const chromeos::quick_pair::LogBuffer::LogMessage& log_message) override;
  void OnLogBufferCleared() override;

  // Message handler callback that returns the Log Buffer in dictionary form.
  void HandleGetLogMessages(const base::ListValue* args);

  // Message handler callback that clears the Log Buffer.
  void ClearLogBuffer(const base::ListValue* args);

  base::ScopedObservation<chromeos::quick_pair::LogBuffer,
                          chromeos::quick_pair::LogBuffer::Observer>
      observation_{this};
  base::WeakPtrFactory<QuickPairHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_QUICK_PAIR_QUICK_PAIR_HANDLER_H_
