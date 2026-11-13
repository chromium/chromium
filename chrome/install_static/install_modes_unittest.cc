// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_modes.h"

#include <windows.h>

#include <cguid.h>

#include <string_view>

#include "base/strings/string_util.h"
#include "chrome/install_static/buildflags.h"
#include "chrome/install_static/install_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Gt;
using ::testing::Le;
using ::testing::Ne;
using ::testing::Not;
using ::testing::ResultOf;
using ::testing::StrEq;
using ::testing::StrNe;

namespace install_static {

namespace {

// A matcher that returns true if |arg| contains a character that is neither
// alphanumeric nor a period.
MATCHER(ContainsIllegalProgIdChar, "") {
  for (wchar_t c : std::wstring_view(arg)) {
    if (!base::IsAsciiAlphaNumeric(c) && c != L'.') {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(InstallModes, VerifyModes) {
  ASSERT_THAT(kInstallModes.size(), Gt(0));
  for (size_t i = 0; i < kInstallModes.size(); ++i) {
    const InstallConstants& mode = kInstallModes[i];

    // The modes must be listed in order.
    ASSERT_THAT(mode.index, Eq(i));

    // The first mode must have no install switch; the rest must have one.
    if (i == 0)
      ASSERT_THAT(mode.install_switch, StrEq(""));
    else
      ASSERT_THAT(mode.install_switch, StrNe(""));

    // The first mode must have no suffix; the rest must have one.
    if (i == 0)
      ASSERT_THAT(mode.install_suffix, StrEq(L""));
    else
      ASSERT_THAT(mode.install_suffix, StrNe(L""));

    // The first mode must have no logo suffix; the rest must have one.
    if (i == 0)
      ASSERT_THAT(mode.logo_suffix, StrEq(L""));
    else
      ASSERT_THAT(mode.logo_suffix, StrNe(L""));

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
    // The modes must have an appguid if Google Update integration is
    // supported.
    ASSERT_THAT(mode.app_guid, StrNe(L""));
#else
    ASSERT_THAT(mode.app_guid, StrEq(L""));
#endif

    // Every mode must have a base app name.
    ASSERT_THAT(mode.base_app_name, StrNe(L""));

    // Every mode must have a base app id, and it must not contain illegal
    // ProgId characters.
    ASSERT_THAT(mode.base_app_id, StrNe(L""));
    ASSERT_THAT(mode.base_app_id, Not(ContainsIllegalProgIdChar()));

    // The Browser ProgID prefix must not be empty, must be no greater than 11
    // characters long, must contain no punctuation, and may not start with a
    // digit (https://msdn.microsoft.com/library/windows/desktop/dd542719.aspx).
    ASSERT_THAT(mode.browser_prog_id_prefix, StrNe(L""));
    ASSERT_THAT(std::wstring_view(mode.browser_prog_id_prefix).size(), Le(11u));
    ASSERT_THAT(mode.browser_prog_id_prefix, Not(ContainsIllegalProgIdChar()));
    ASSERT_THAT(*mode.browser_prog_id_prefix, ResultOf(iswdigit, Eq(0)));

    // Test the same things for PDF ProgID prefix.
    ASSERT_THAT(mode.pdf_prog_id_prefix, StrNe(L""));
    ASSERT_THAT(std::wstring_view(mode.pdf_prog_id_prefix).size(), Le(11u));
    ASSERT_THAT(mode.pdf_prog_id_prefix, Not(ContainsIllegalProgIdChar()));
    ASSERT_THAT(*mode.pdf_prog_id_prefix, ResultOf(iswdigit, Eq(0)));

    // The ProgID description must not be empty.
    ASSERT_THAT(mode.browser_prog_id_description, StrNe(L""));

    // The PDFProgID description also must not be empty.
    ASSERT_THAT(mode.pdf_prog_id_description, StrNe(L""));

    // Every mode must have an Active Setup GUID.
    ASSERT_THAT(mode.active_setup_guid, StrNe(L""));

    // Every mode must have a toast activator CLSID.
    ASSERT_THAT(mode.toast_activator_clsid, Ne(CLSID_NULL));

    // Every mode must have an elevator CLSID.
    ASSERT_THAT(mode.elevator_clsid, Ne(CLSID_NULL));

    // Every mode must have an elevator IID.
    ASSERT_THAT(mode.elevator_iid, Ne(CLSID_NULL));

    // Tracing service CLSID and IID must either both be set or both be null,
    // and must be set if system-level installs are supported.
    ASSERT_EQ(mode.tracing_service_clsid == CLSID_NULL,
              mode.tracing_service_iid == CLSID_NULL);
    if (mode.supports_system_level) {
      ASSERT_THAT(mode.tracing_service_clsid, Ne(CLSID_NULL));
      ASSERT_THAT(mode.tracing_service_iid, Ne(CLSID_NULL));
    }

    // COM CLSIDs and IIDs must be unique across modes.
    for (size_t j = 0; j < i; ++j) {
      const InstallConstants& prior_mode = kInstallModes[j];
      ASSERT_THAT(mode.toast_activator_clsid,
                  Ne(prior_mode.toast_activator_clsid));
      ASSERT_THAT(mode.elevator_clsid, Ne(prior_mode.elevator_clsid));
      ASSERT_THAT(mode.elevator_iid, Ne(prior_mode.elevator_iid));
      if (mode.tracing_service_clsid != CLSID_NULL) {
        ASSERT_THAT(mode.tracing_service_clsid,
                    Ne(prior_mode.tracing_service_clsid));
        ASSERT_THAT(mode.tracing_service_iid,
                    Ne(prior_mode.tracing_service_iid));
      }
    }

    // Assert that html_doc_icon_resource_index is set.
    ASSERT_THAT(mode.html_doc_icon_resource_index, Ne(0));

    // Assert that pdf_doc_icon_resource_index is set.
    ASSERT_THAT(mode.pdf_doc_icon_resource_index, Ne(0));

    // Every mode must specify a direct launch URL scheme; empty string is okay.
    ASSERT_THAT(mode.direct_launch_url_scheme, Ne(nullptr));

    // UNSUPPORTED and USE_GOOGLE_UPDATE_INTEGRATION are mutually exclusive.
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
    ASSERT_THAT(mode.channel_strategy, Eq(ChannelStrategy::UNSUPPORTED));
#endif
  }
}

TEST(InstallModes, GetClientsKeyPath) {
  constexpr wchar_t kAppGuid[] = L"test";

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  ASSERT_THAT(GetClientsKeyPath(kAppGuid),
              StrEq(L"Software\\Google\\Update\\Clients\\test"));
#else
  ASSERT_THAT(GetClientsKeyPath(kAppGuid),
              StrEq(std::wstring(L"Software\\").append(kProductPathName)));
#endif
}

TEST(InstallModes, GetClientStateKeyPath) {
  constexpr wchar_t kAppGuid[] = L"test";

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  ASSERT_THAT(GetClientStateKeyPath(kAppGuid),
              StrEq(L"Software\\Google\\Update\\ClientState\\test"));
#else
  ASSERT_THAT(GetClientStateKeyPath(kAppGuid),
              StrEq(std::wstring(L"Software\\").append(kProductPathName)));
#endif
}

TEST(InstallModes, GetClientStateMediumKeyPath) {
  constexpr wchar_t kAppGuid[] = L"test";

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  ASSERT_THAT(GetClientStateMediumKeyPath(kAppGuid),
              StrEq(L"Software\\Google\\Update\\ClientStateMedium\\test"));
#else
  ASSERT_THAT(GetClientStateMediumKeyPath(kAppGuid),
              StrEq(std::wstring(L"Software\\").append(kProductPathName)));
#endif
}

}  // namespace install_static
