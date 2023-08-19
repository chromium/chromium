// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/target_utils.h"

#include <memory>
#include <utility>

#include "base/threading/platform_thread.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_info.h"
#include "chrome/test/chromedriver/net/timeout.h"

Status target_utils::GetWebViewsInfo(DevToolsClient& devtools_websocket_client,
                                     const Timeout* timeout,
                                     WebViewsInfo& views_info) {
  Status status{kOk};

  base::Value::Dict params;
  base::Value::Dict result;
  status = devtools_websocket_client.SendCommandAndGetResultWithTimeout(
      "Target.getTargets", params, timeout, &result);
  if (status.IsError()) {
    return status;
  }
  base::Value* target_infos = result.Find("targetInfos");
  if (!target_infos) {
    return Status(
        kUnknownError,
        "result of call to Target.getTargets does not contain targetInfos");
  }
  if (!target_infos->is_list()) {
    return Status(kUnknownError,
                  "targetInfos in Target.getTargets response is not a list");
  }
  return views_info.FillFromTargetsInfo(target_infos->GetList());
}

Status target_utils::WaitForPage(DevToolsClient& client,
                                 const Timeout& timeout) {
  do {
    WebViewsInfo views_info;
    Status status = GetWebViewsInfo(client, &timeout, views_info);
    if (status.IsError()) {
      return status;
    }
    if (views_info.ContainsTargetType(WebViewInfo::kPage)) {
      return Status(kOk);
    }
    base::PlatformThread::Sleep(base::Milliseconds(50));
  } while (!timeout.IsExpired());
  return Status(kTimeout, "unable to discover open pages");
}

Status target_utils::AttachToPageTarget(
    DevToolsClient& browser_client,
    const std::string& target_id,
    const Timeout* timeout,
    std::unique_ptr<DevToolsClient>& target_client) {
  base::Value::Dict params;
  base::Value::Dict result;
  params.Set("targetId", target_id);
  params.Set("flatten", true);
  Status status = browser_client.SendCommandAndGetResultWithTimeout(
      "Target.attachToTarget", params, timeout, &result);
  if (status.IsError()) {
    return status;
  }

  std::string* session_id_ptr = result.FindString("sessionId");
  if (session_id_ptr == nullptr) {
    return Status(kUnknownError,
                  "No sessionId in the response to Target.attachToTarget");
  }

  std::unique_ptr<DevToolsClientImpl> client =
      std::make_unique<DevToolsClientImpl>(target_id, *session_id_ptr);
  client->SetMainPage(true);
  target_client = std::move(client);

  return status;
}
