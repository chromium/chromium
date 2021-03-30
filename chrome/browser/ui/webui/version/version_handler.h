// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

// Handler class for Version page operations.
class VersionHandler : public content::WebUIMessageHandler {
 public:
  VersionHandler();
  ~VersionHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestVersionInfo" message sent by |chrome.send| in JS.
  // This is still supported for platform-specific asynchronous calls (see
  // derived classes) but the main version information is now retrieved with
  // below messages using |cr.sendWithPromise|.
  virtual void HandleRequestVersionInfo(const base::ListValue* args);

  // Callback for the "requestVariationInfo" message. This resolves immediately
  // with variations list as well as command variations if requested.
  virtual void HandleRequestVariationInfo(const base::ListValue* args);

  // Callback for the "requestPathInfo" message. This resolves asynchronously
  // with |OnGotFilePaths|.
  virtual void HandleRequestPathInfo(const base::ListValue* args);

 private:
  // Callback which handles returning the executable and profile paths to the
  // front end.
  void OnGotFilePaths(std::string callback_id,
                      std::u16string* executable_path_data,
                      std::u16string* profile_path_data);

  // Factory for the creating refs in callbacks.
  base::WeakPtrFactory<VersionHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VersionHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_HANDLER_H_
