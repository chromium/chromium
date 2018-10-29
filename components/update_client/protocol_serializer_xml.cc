// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_serializer_xml.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/update_client/updater_state.h"

namespace update_client {

std::string ProtocolSerializerXml::Serialize(
    const protocol_request::Request& request) const {
  std::string msg;
  base::StringAppendF(&msg,
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                      "<request protocol=\"%s\"",
                      kProtocolVersion);

  // Constant information for this updater.
  base::StringAppendF(&msg, " dedup=\"cr\" acceptformat=\"crx2,crx3\"");

  if (!request.additional_attributes.empty()) {
    for (const auto& attr : request.additional_attributes) {
      base::StringAppendF(&msg, " %s=\"%s\"", attr.first.c_str(),
                          attr.second.c_str());
    }
  }

  // Session id and request id.
  base::StringAppendF(&msg, " sessionid=\"%s\" requestid=\"%s\"",
                      request.session_id.c_str(), request.request_id.c_str());

  // Chrome version and platform information.
  base::StringAppendF(&msg,
                      " updater=\"%s\" updaterversion=\"%s\" prodversion=\"%s\""
                      " lang=\"%s\" os=\"%s\" arch=\"%s\" nacl_arch=\"%s\"",
                      request.updatername.c_str(),
                      request.updaterversion.c_str(),
                      request.prodversion.c_str(), request.lang.c_str(),
                      request.operating_system.c_str(), request.arch.c_str(),
                      request.nacl_arch.c_str());
#if defined(OS_WIN)
  if (request.is_wow64)
    base::StringAppendF(&msg, " wow64=\"%d\"", request.is_wow64);
#endif  // OS_WIN
  if (!request.updaterchannel.empty()) {
    base::StringAppendF(&msg, " updaterchannel=\"%s\"",
                        request.updaterchannel.c_str());
  }
  if (!request.prodchannel.empty()) {
    base::StringAppendF(&msg, " prodchannel=\"%s\"",
                        request.prodchannel.c_str());
  }
  if (!request.dlpref.empty())
    base::StringAppendF(&msg, " dlpref=\"%s\"", request.dlpref.c_str());

  if (request.domain_joined) {
    base::StringAppendF(&msg, " %s=\"%d\"", UpdaterState::kIsEnterpriseManaged,
                        *request.domain_joined);
  }
  base::StringAppendF(&msg, ">");

  // HW platform information.
  base::StringAppendF(&msg, "<hw physmemory=\"%d\"/>", request.hw.physmemory);

  // OS version and platform information.
  base::StringAppendF(&msg, "<os platform=\"%s\" arch=\"%s\"",
                      request.os.platform.c_str(), request.os.arch.c_str());
  if (!request.os.version.empty())
    base::StringAppendF(&msg, " version=\"%s\"", request.os.version.c_str());
  if (!request.os.service_pack.empty())
    base::StringAppendF(&msg, " sp=\"%s\"", request.os.service_pack.c_str());
  base::StringAppendF(&msg, "/>");

#if defined(GOOGLE_CHROME_BUILD)
  if (request.updater) {
    const auto& updater = *request.updater;
    base::StringAppendF(&msg, "<updater name=\"%s\"", updater.name.c_str());
    if (!updater.version.empty())
      base::StringAppendF(&msg, " version=\"%s\"", updater.version.c_str());
    if (updater.last_checked)
      base::StringAppendF(&msg, " lastchecked=\"%d\"", *updater.last_checked);
    if (updater.last_started)
      base::StringAppendF(&msg, " laststarted=\"%d\"", *updater.last_started);
    base::StringAppendF(
        &msg,
        " ismachine=\"%d\" autoupdatecheckenabled=\"%d\" updatepolicy=\"%d\"/>",
        updater.is_machine, updater.autoupdate_check_enabled,
        updater.update_policy);
  }
#endif

  for (const auto& app : request.apps) {
    base::StringAppendF(&msg, "<app appid=\"%s\"", app.app_id.c_str());
    base::StringAppendF(&msg, " version=\"%s\"", app.version.c_str());
    if (!app.brand_code.empty())
      base::StringAppendF(&msg, " brand=\"%s\"", app.brand_code.c_str());
    if (!app.install_source.empty()) {
      base::StringAppendF(&msg, " installsource=\"%s\"",
                          app.install_source.c_str());
    }
    if (!app.install_location.empty()) {
      base::StringAppendF(&msg, " installedby=\"%s\"",
                          app.install_location.c_str());
    }
    for (const auto& attr : app.installer_attributes) {
      base::StringAppendF(&msg, " %s=\"%s\"", attr.first.c_str(),
                          attr.second.c_str());
    }
    if (!app.cohort.empty())
      base::StringAppendF(&msg, " cohort=\"%s\"", app.cohort.c_str());
    if (!app.cohort_name.empty())
      base::StringAppendF(&msg, " cohortname=\"%s\"", app.cohort_name.c_str());
    if (!app.cohort_hint.empty())
      base::StringAppendF(&msg, " cohorthint=\"%s\"", app.cohort_hint.c_str());

    if (app.enabled)
      base::StringAppendF(&msg, " enabled=\"%d\"", *app.enabled ? 1 : 0);
    if (app.disabled_reasons) {
      for (const int disabled_reason : *app.disabled_reasons)
        base::StringAppendF(&msg, "<disabled reason=\"%d\"/>", disabled_reason);
    }

    base::StringAppendF(&msg, ">");

    if (app.update_check) {
      base::StringAppendF(&msg, "<updatecheck");
      if (app.update_check->is_update_disabled)
        base::StringAppendF(&msg, " updatedisabled=\"true\"");
      base::StringAppendF(&msg, "/>");
    }

    if (app.ping) {
      base::StringAppendF(&msg, "<ping");

      // Output "ad" or "a" only if the this app has been seen 'active'.
      if (app.ping->date_last_active) {
        base::StringAppendF(&msg, " ad=\"%d\"", *app.ping->date_last_active);
      } else if (app.ping->days_since_last_active_ping) {
        base::StringAppendF(&msg, " a=\"%d\"",
                            *app.ping->days_since_last_active_ping);
      }

      // Output "rd" if valid or "r" as a last resort roll call metric.
      if (app.ping->date_last_roll_call)
        base::StringAppendF(&msg, " rd=\"%d\"", *app.ping->date_last_roll_call);
      else
        base::StringAppendF(&msg, " r=\"%d\"",
                            app.ping->days_since_last_roll_call);
      if (!app.ping->ping_freshness.empty())
        base::StringAppendF(&msg, " ping_freshness=\"%s\"",
                            app.ping->ping_freshness.c_str());

      base::StringAppendF(&msg, "/>");  // End <ping>.
    }

    if (!app.fingerprint.empty()) {
      base::StringAppendF(&msg,
                          "<packages>"
                          "<package fp=\"%s\"/>"
                          "</packages>",
                          app.fingerprint.c_str());
    }

    if (app.events) {
      for (const auto& event : *app.events) {
        DCHECK(event.is_dict());
        DCHECK(!event.DictEmpty());
        base::StringAppendF(&msg, "<event");
        const auto& attrs = event.DictItems();
        for (auto it = attrs.begin(); it != attrs.end(); ++it) {
          base::StringAppendF(&msg, " %s=", (*it).first.c_str());
          const auto& value = (*it).second;
          if (value.is_string())
            base::StringAppendF(&msg, "\"%s\"", value.GetString().c_str());
          else if (value.is_int())
            base::StringAppendF(&msg, "\"%d\"", value.GetInt());
          else
            NOTREACHED();
        }
        base::StringAppendF(&msg, "/>");
      }
    }
    base::StringAppendF(&msg, "</app>");
  }

  base::StringAppendF(&msg, "</request>");
  return msg;
}

}  // namespace update_client
