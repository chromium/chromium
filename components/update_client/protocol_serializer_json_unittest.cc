// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_serializer_json.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/test_activity_data_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace update_client {

TEST(SerializeRequestJSON, Serialize) {
  // When no updater state is provided, then check that the elements and
  // attributes related to the updater state are not serialized.

  base::test::TaskEnvironment env;

  {
    auto pref = std::make_unique<TestingPrefServiceSimple>();
    RegisterPersistedDataPrefs(pref->registry());
    auto metadata = CreatePersistedData(pref.get(), nullptr);
    std::vector<std::string> items = {"id1"};
    test::SetDateLastData(metadata.get(), items, 1234);

    std::vector<base::Value::Dict> events(2);
    events[0].Set("a", 1);
    events[0].Set("b", "2");
    events[1].Set("error", 0);

    std::vector<protocol_request::App> apps;
    apps.push_back(MakeProtocolApp(
        "id1", base::Version("1.0"), "ap1", "BRND", "lang", -1, "source1",
        "location1", "fp1", {{"attr1", "1"}, {"attr2", "2"}}, "c1", "ch1",
        "cn1", "test", {0, 1},
        MakeProtocolUpdateCheck(true, "33.12", true, false),
        {{"install", "foobar_install_data_index", ""}},
        MakeProtocolPing("id1", metadata.get(), {}), std::move(events)));

    const auto request = std::make_unique<ProtocolSerializerJSON>()->Serialize(
        MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}",
                            "prod_id", "1.0", "channel", "OS", "cacheable",
                            std::nullopt, {{"extra", "params"}}, {},
                            std::move(apps)));
    constexpr char regex[] =
        R"({"request":{"@os":"\w+","@updater":"prod_id",)"
        R"("acceptformat":"crx3,puff",)"
        R"("app":\[{"ap":"ap1","appid":"id1","attr1":"1","attr2":"2",)"
        R"("brand":"BRND","cohort":"c1","cohorthint":"ch1","cohortname":"cn1",)"
        R"("data":\[{"index":"foobar_install_data_index","name":"install"}],)"
        R"("disabled":\[{"reason":0},{"reason":1}],"enabled":false,)"
        R"("event":\[{"a":1,"b":"2"},{"error":0}],)"
        R"("installdate":-1,)"
        R"("installedby":"location1","installsource":"source1",)"
        R"("lang":"lang",)"
        R"("packages":{"package":\[{"fp":"fp1"}]},)"
        R"("ping":{"ping_freshness":"{[-\w]{36}}","rd":1234},)"
        R"("release_channel":"test",)"
        R"("updatecheck":{"rollback_allowed":true,)"
        R"("targetversionprefix":"33.12",)"
        R"("updatedisabled":true},"version":"1.0"}],"arch":"\w+","dedup":"cr",)"
        R"("dlpref":"cacheable","extra":"params","hw":{"avx":(true|false),)"
        R"("physmemory":\d+,"sse":(true|false),"sse2":(true|false),)"
        R"("sse3":(true|false),"sse41":(true|false),"sse42":(true|false),)"
        R"("ssse3":(true|false)},)"
        R"("ismachine":false,"nacl_arch":"[-\w]+",)"
        R"("os":{"arch":"[_,-.\w]+","platform":"OS",)"
        R"(("sp":"[\s\w]+",)?"version":"[+-.\w]+"},"prodchannel":"channel",)"
        R"("prodversion":"1.0","protocol":"3.1","requestid":"{[-\w]{36}}",)"
        R"("sessionid":"{[-\w]{36}}","updaterchannel":"channel",)"
        R"("updaterversion":"1.0"(,"wow64":true)?}})";
    EXPECT_TRUE(RE2::FullMatch(request, regex)) << request << "\n VS \n"
                                                << regex;
  }
  {
    // Tests `sameversionupdate` presence with a minimal request for one app.
    std::vector<protocol_request::App> apps;
    apps.push_back(MakeProtocolApp(
        "id1", base::Version("1.0"), "", "", "", -2, "", "", "", {}, "", "", "",
        "", {}, MakeProtocolUpdateCheck(false, "", false, true), {},
        std::nullopt, std::nullopt));

    const auto request = std::make_unique<ProtocolSerializerJSON>()->Serialize(
        MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                            "", "", "", "", std::nullopt, {}, {},
                            std::move(apps)));

    constexpr char regex[] =
        R"("app":\[{"appid":"id1","enabled":true,)"
        R"("updatecheck":{"sameversionupdate":true},"version":"1.0"}])";
    EXPECT_TRUE(RE2::PartialMatch(request, regex)) << request << "\n VS \n"
                                                   << regex;
  }
}

TEST(SerializeRequestJSON, DownloadPreference) {
  base::test::TaskEnvironment env;
  // Verifies that an empty |download_preference| is not serialized.
  const auto serializer = std::make_unique<ProtocolSerializerJSON>();
  auto request = serializer->Serialize(
      MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                          "", "", "", "", std::nullopt, {}, {}, {}));
  EXPECT_FALSE(RE2::PartialMatch(request, R"("dlpref":)")) << request;

  // Verifies that |download_preference| is serialized.
  request = serializer->Serialize(
      MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                          "", "", "", "cacheable", std::nullopt, {}, {}, {}));
  EXPECT_TRUE(RE2::PartialMatch(request, R"("dlpref":"cacheable")")) << request;
}

// When present, updater state attributes are only serialized for Google builds,
// except the |domainjoined| attribute, which is serialized in all cases.
TEST(SerializeRequestJSON, UpdaterStateAttributes) {
  base::test::TaskEnvironment env;
  const auto serializer = std::make_unique<ProtocolSerializerJSON>();

  const auto request = serializer->Serialize(MakeProtocolRequest(
      true, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "prod_id", "1.0",
      "channel", "OS", "cacheable", true, {{"extra", "params"}},
      {{"ismachine", "1"},
       {"name", "Omaha"},
       {"version", "1.2.3.4"},
       {"laststarted", "1"},
       {"lastchecked", "2"},
       {"autoupdatecheckenabled", "0"},
       {"updatepolicy", "-1"}},
      {}));
  constexpr char regex[] =
      R"({"request":{"@os":"\w+","@updater":"prod_id",)"
      R"("acceptformat":"crx3,puff","arch":"\w+","dedup":"cr",)"
      R"("dlpref":"cacheable","domainjoined":true,"extra":"params",)"
      R"("hw":{"avx":(true|false),)"
      R"("physmemory":\d+,"sse":(true|false),"sse2":(true|false),)"
      R"("sse3":(true|false),"sse41":(true|false),"sse42":(true|false),)"
      R"("ssse3":(true|false)},)"
      R"("ismachine":true,)"
      R"("nacl_arch":"[-\w]+",)"
      R"("os":{"arch":"[,-.\w]+","platform":"OS",("sp":"[\s\w]+",)?)"
      R"("version":"[+-.\w]+"},"prodchannel":"channel","prodversion":"1.0",)"
      R"("protocol":"3.1","requestid":"{[-\w]{36}}","sessionid":"{[-\w]{36}}",)"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      R"("updater":{"autoupdatecheckenabled":false,"ismachine":true,)"
      R"("lastchecked":2,"laststarted":1,"name":"Omaha","updatepolicy":-1,)"
      R"("version":"1\.2\.3\.4"},)"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      R"("updaterchannel":"channel","updaterversion":"1.0"(,"wow64":true)?}})";
  EXPECT_TRUE(RE2::FullMatch(request, regex)) << request << "\n VS \n" << regex;
}

TEST(SerializeRequestJSON, DomainJoined) {
  base::test::TaskEnvironment env;

  const auto serializer = std::make_unique<ProtocolSerializerJSON>();
  std::string request = serializer->Serialize(
      MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                          "", "", "", "", std::nullopt, {}, {}, {}));
  EXPECT_FALSE(RE2::PartialMatch(request, R"("domainjoined")")) << request;

  request = serializer->Serialize(
      MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                          "", "", "", "", true, {}, {}, {}));
  EXPECT_TRUE(RE2::PartialMatch(request, R"("domainjoined":true)")) << request;

  request = serializer->Serialize(
      MakeProtocolRequest(false, "{15160585-8ADE-4D3C-839B-1281A6035D1F}", "",
                          "", "", "", "", false, {}, {}, {}));
  EXPECT_TRUE(RE2::PartialMatch(request, R"("domainjoined":false)")) << request;
}

}  // namespace update_client
