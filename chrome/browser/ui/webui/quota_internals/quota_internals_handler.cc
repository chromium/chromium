// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals/quota_internals_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_proxy.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_types.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"

using content::BrowserContext;

namespace quota_internals {

QuotaInternalsHandler::QuotaInternalsHandler() {}

QuotaInternalsHandler::~QuotaInternalsHandler() {
  if (proxy_.get())
    proxy_->handler_ = NULL;
}

void QuotaInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestInfo", base::BindRepeating(&QuotaInternalsHandler::OnRequestInfo,
                                         base::Unretained(this)));
}

void QuotaInternalsHandler::ReportAvailableSpace(int64_t available_space) {
  SendMessage("AvailableSpaceUpdated",
              base::Value(static_cast<double>(available_space)));
}

void QuotaInternalsHandler::ReportGlobalInfo(const GlobalStorageInfo& data) {
  std::unique_ptr<base::Value> value(data.NewValue());
  SendMessage("GlobalInfoUpdated", *value);
}

void QuotaInternalsHandler::ReportPerHostInfo(
    const std::vector<PerHostStorageInfo>& hosts) {
  base::ListValue values;
  for (auto itr(hosts.begin()); itr != hosts.end(); ++itr) {
    values.Append(itr->NewValue());
  }

  SendMessage("PerHostInfoUpdated", values);
}

void QuotaInternalsHandler::ReportPerOriginInfo(
    const std::vector<PerOriginStorageInfo>& origins) {
  base::ListValue origins_value;
  for (auto itr(origins.begin()); itr != origins.end(); ++itr) {
    origins_value.Append(itr->NewValue());
  }

  SendMessage("PerOriginInfoUpdated", origins_value);
}

void QuotaInternalsHandler::ReportStatistics(const Statistics& stats) {
  base::DictionaryValue dict;
  for (auto itr(stats.begin()); itr != stats.end(); ++itr) {
    dict.SetString(itr->first, itr->second);
  }

  SendMessage("StatisticsUpdated", dict);
}

void QuotaInternalsHandler::SendMessage(const std::string& message,
                                        const base::Value& value) {
  web_ui()->CallJavascriptFunctionUnsafe("cr.quota.messageHandler",
                                         base::Value(message), value);
}

void QuotaInternalsHandler::OnRequestInfo(const base::ListValue*) {
  if (!proxy_.get())
    proxy_ = new QuotaInternalsProxy(this);
  proxy_->RequestInfo(
      BrowserContext::GetDefaultStoragePartition(
          Profile::FromWebUI(web_ui()))->GetQuotaManager());
}

}  // namespace quota_internals
