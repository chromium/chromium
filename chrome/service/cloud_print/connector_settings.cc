// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/connector_settings.h"

#include <stddef.h>

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/service/cloud_print/print_system.h"
#include "chrome/service/service_process_prefs.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"

namespace {

const char kDeleteOnEnumFail[] = "delete_on_enum_fail";
const char kName[] = "name";
const char kConnect[] = "connect";

}  // namespace

namespace cloud_print {

ConnectorSettings::ConnectorSettings()
    : delete_on_enum_fail_(false),
      connect_new_printers_(true),
      xmpp_ping_enabled_(false),
      xmpp_ping_timeout_sec_(kDefaultXmppPingTimeoutSecs) {
}

ConnectorSettings::~ConnectorSettings() {
}

void ConnectorSettings::InitFrom(ServiceProcessPrefs* prefs) {
  CopyFrom(ConnectorSettings());

  proxy_id_ = prefs->GetString(prefs::kCloudPrintProxyId, std::string());
  if (proxy_id_.empty()) {
    proxy_id_ = PrintSystem::GenerateProxyId();
    prefs->SetString(prefs::kCloudPrintProxyId, proxy_id_);
    prefs->WritePrefs();
  }

  // Getting print system specific settings from the preferences.
  const base::DictionaryValue* print_system_settings =
      prefs->GetDictionary(prefs::kCloudPrintPrintSystemSettings);
  if (print_system_settings) {
    print_system_settings_.reset(print_system_settings->DeepCopy());
    // TODO(vitalybuka) : Consider to rename and move out option from
    // print_system_settings.
    print_system_settings_->GetBoolean(kDeleteOnEnumFail,
                                       &delete_on_enum_fail_);
  }

  // Check if there is an override for the cloud print server URL.
  server_url_ = cloud_devices::GetCloudPrintURL();
  DCHECK(server_url_.is_valid());

  connect_new_printers_ = prefs->GetBoolean(
      prefs::kCloudPrintConnectNewPrinters, true);

  xmpp_ping_enabled_ = prefs->GetBoolean(
      prefs::kCloudPrintXmppPingEnabled, false);
  int timeout = prefs->GetInt(
      prefs::kCloudPrintXmppPingTimeout, kDefaultXmppPingTimeoutSecs);
  SetXmppPingTimeoutSec(timeout);

  const base::ListValue* printers = prefs->GetList(prefs::kCloudPrintPrinters);
  if (printers) {
    for (size_t i = 0; i < printers->GetSize(); ++i) {
      const base::DictionaryValue* dictionary = NULL;
      if (printers->GetDictionary(i, &dictionary) && dictionary) {
        std::string name;
        dictionary->GetString(kName, &name);
        if (!name.empty()) {
          bool connect = connect_new_printers_;
          dictionary->GetBoolean(kConnect, &connect);
          if (connect != connect_new_printers_)
            printers_.insert(name);
        }
      }
    }
  }
  if (connect_new_printers_) {
    UMA_HISTOGRAM_COUNTS_10000("CloudPrint.PrinterBlacklistSize",
                               printers_.size());
  } else {
    UMA_HISTOGRAM_COUNTS_10000("CloudPrint.PrinterWhitelistSize",
                               printers_.size());
  }
}

bool ConnectorSettings::ShouldConnect(const std::string& printer_name) const {
  auto printer = printers_.find(printer_name);
  if (printer != printers_.end())
    return !connect_new_printers_;
  return connect_new_printers_;
}

void ConnectorSettings::CopyFrom(const ConnectorSettings& source) {
  server_url_ = source.server_url();
  proxy_id_ = source.proxy_id();
  delete_on_enum_fail_ = source.delete_on_enum_fail();
  connect_new_printers_ = source.connect_new_printers_;
  xmpp_ping_enabled_ = source.xmpp_ping_enabled();
  xmpp_ping_timeout_sec_ = source.xmpp_ping_timeout_sec();
  printers_ = source.printers_;
  if (source.print_system_settings())
    print_system_settings_.reset(source.print_system_settings()->DeepCopy());
}

void ConnectorSettings::SetXmppPingTimeoutSec(int timeout) {
  xmpp_ping_timeout_sec_ = timeout;
  if (xmpp_ping_timeout_sec_ < kMinXmppPingTimeoutSecs) {
    LOG(WARNING) <<
        "CP_CONNECTOR: XMPP ping timeout is less than minimal value";
    xmpp_ping_timeout_sec_ = kMinXmppPingTimeoutSecs;
  }
}

}  // namespace cloud_print
