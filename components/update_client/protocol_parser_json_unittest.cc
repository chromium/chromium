// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser_json.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

const char* kJSONValid = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"pipeline_id": "pipe1",
        "operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3"}
       ]}
      ]
     }
    }
   ]
  }})";

const char* kJSONHash = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [
          {"url": "http://example.com/extension_1_2_3_4.crx"}
         ],
         "out": {"sha256": "1234"}},
        {"type": "crx3",
         "in": {"sha256": "1234"}}]}
      ]
     }
    }
   ]
  }})";

const char* kJSONInvalidSizes = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines": [
       {"operations":[
         {"type": "download",
          "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}],
          "size": 1234},
         {"type": "download",
          "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}],
          "size": 9007199254740991},
         {"type": "download",
          "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}],
          "size": -1234},
         {"type": "download",
          "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
         {"type": "download",
          "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}],
          "size": "-a"},
         {"type": "download",
          "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}],
          "size": -123467890123456789},
         {"type": "download",
          "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}],
          "size": 123467890123456789}
      ]}]
     }
    }
   ]
  }})";

const char* kJSONInvalidMissingUrl = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "out": {"sha256": "1234"}}
        {"type": "crx3",
         "in": {"sha256": "1234"}}]}
      ]
     }
    }
   ]
  }})";

const char* kJSONInvalidMissingUpdateCheck = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok"
    }
   ]
  }})";

// `updatecheck` is supposed to be a dictionary. It is a list here.
const char* kJSONInvalidUpdateCheck = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {
      "appid":"12345",
      "status":"ok",
      "updatecheck": []
    }
   ]
  }})";

const char* kJSONMissingAppId = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3"}
       ]}
      ]
     }
    }
   ]
  }})";

const char* kJSONInvalidCodebase = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "nonurl"}]},
        {"type": "crx3"}
       ]}
      ]
     }
    }
   ]
  }})";

const char* kJSONMissingVersion = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3"}
       ]}
      ]
     }
    }
   ]
  }})";

const char* kJSONInvalidVersion = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.a",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3"}
       ]}
      ]
     }
    }
   ]
  }})";

// Includes a <daystart> tag.
const char* kJSONWithDaystart = R"()]}'
  {"response":{
   "protocol":"4.0",
   "daystart":{"elapsed_days":456},
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3"}
       ]}
      ]
     }
    }
   ]
  }})";

// Indicates no updates available.
const char* kJSONNoUpdate = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"noupdate"
     }
    }
   ]
  }})";

// Includes two app objects, one app with an error.
const char* kJSONTwoAppsOneError = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"aaaaaaaa",
     "status":"error-unknownApplication"
    },
    {"appid":"bbbbbbbb",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3"}
       ]}
      ]
     }
    }
   ]
  }})";

// Includes two <app> tags, both of which set the cohort.
const char* kJSONTwoAppsSetCohort = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"aaaaaaaa",
     "cohort":"1:2q3/",
     "status":"ok",
     "updatecheck":{
      "status":"noupdate"
     }
    },
    {"appid":"bbbbbbbb",
     "cohort":"1:33z@0.33",
     "cohortname":"cname",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3"}
       ]}
      ]
     }
    }
   ]
  }})";

// Includes a run action for an update check with status='ok'.
const char* kJSONUpdateCheckStatusOkWithRunAction = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3"},
        {"type": "run",
         "path": "file.exe"}
       ]}
      ]
     }
    }
   ]
  }})";

// Includes nine app objects with status different than 'ok'.
const char* kJSONAppsStatusError = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"aaaaaaaa", "status":"error-unknownApplication"},
    {"appid":"bbbbbbbb", "status":"restricted"},
    {"appid":"cccccccc", "status":"error-invalidAppId"},
    {"appid":"dddddddd", "status":"error-osnotsupported"},
    {"appid":"eeeeeeee", "status":"error-hwnotsupported"},
    {"appid":"ffffffff", "status":"error-hash"},
    {"appid":"gggggggg", "status":"error-unsupportedprotocol"},
    {"appid":"hhhhhhhh", "status":"error-internal"},
    {"appid":"iiiiiiii", "status":"foobar"}
   ]
  }})";

// Includes a manifest |run| value for an update check with status='ok'. Also
// includes install data in the `data` element.
const char* kJSONManifestRun = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "data":[{
      "status":"ok",
      "name":"install",
      "index":"foobar_install_data_index",
      "#text":"sampledata"
     }],
     "updatecheck":{
      "status":"ok",
      "nextversion":"1.2.3.4",
      "pipelines":[
       {"operations":[
        {"type": "download",
         "urls": [{"url": "http://example.com/extension_1_2_3_4.crx"}]},
        {"type": "crx3",
         "path":"UpdaterSetup.exe",
         "arguments":"--arg1 --arg2"
        }
       ]}
      ]
     }
    }
   ]
  }})";

// Includes two custom response attributes in the update_check.
const char* kJSONCustomAttributes = R"()]}'
  {"response":{
   "protocol":"4.0",
   "apps":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
       "_example1":"example_value1",
       "_example2":"example_value2",
       "_example_bad": {"value": "bad-non-string-value"},
       "_example_bad2": 15,
       "status":"noupdate"
     }
    }
   ]
  }})";

TEST(UpdateClientProtocolParserJSONTest, Parse) {
  const auto parser = std::make_unique<ProtocolParserJSON>();

  // Test parsing of a number of invalid JSON cases
  EXPECT_FALSE(parser->Parse(std::string()));
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONMissingAppId));
  EXPECT_TRUE(parser->results().apps.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONInvalidCodebase));
  EXPECT_TRUE(parser->results().apps.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONMissingVersion));
  EXPECT_TRUE(parser->results().apps.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONInvalidVersion));
  EXPECT_TRUE(parser->results().apps.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONInvalidMissingUpdateCheck));
  EXPECT_TRUE(parser->results().apps.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONInvalidUpdateCheck));
  EXPECT_TRUE(parser->results().apps.empty());
  EXPECT_FALSE(parser->errors().empty());

  {
    // Parse some valid XML, and check that all params came out as expected.
    EXPECT_TRUE(parser->Parse(kJSONValid)) << parser->errors();
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    ASSERT_EQ(1u, parser->results().apps.size());
    const auto* first_result = &parser->results().apps[0];
    EXPECT_STREQ("ok", first_result->status.c_str());
    ASSERT_EQ(1u, first_result->pipelines.size());
    EXPECT_EQ(first_result->pipelines[0].pipeline_id, "pipe1");
    ASSERT_EQ(2u, first_result->pipelines[0].operations.size());
    ASSERT_EQ(1u, first_result->pipelines[0].operations[0].urls.size());
    EXPECT_EQ(GURL("http://example.com/extension_1_2_3_4.crx"),
              first_result->pipelines[0].operations[0].urls[0]);
    EXPECT_EQ(base::Version("1.2.3.4"), first_result->nextversion);
  }
  {
    // Parse xml with hash value.
    EXPECT_TRUE(parser->Parse(kJSONHash));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    EXPECT_FALSE(parser->results().apps.empty());
    const auto* first_result = &parser->results().apps[0];
    EXPECT_EQ("1234", first_result->pipelines[0].operations[0].sha256_out);
  }
  {
    // Parse xml with package size value.
    EXPECT_TRUE(parser->Parse(kJSONInvalidSizes)) << parser->errors();
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    EXPECT_FALSE(parser->results().apps.empty());
    const auto* first_result = &parser->results().apps[0];
    EXPECT_EQ(first_result->pipelines[0].operations.size(), 7u);
    EXPECT_EQ(1234, first_result->pipelines[0].operations[0].size);
    EXPECT_EQ(9007199254740991, first_result->pipelines[0].operations[1].size);
    EXPECT_EQ(0, first_result->pipelines[0].operations[2].size);
    EXPECT_EQ(0, first_result->pipelines[0].operations[3].size);
    EXPECT_EQ(0, first_result->pipelines[0].operations[4].size);
    EXPECT_EQ(0, first_result->pipelines[0].operations[5].size);
    EXPECT_EQ(0, first_result->pipelines[0].operations[6].size);
  }
  {
    // Parse xml with a <daystart> element.
    EXPECT_TRUE(parser->Parse(kJSONWithDaystart));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    EXPECT_FALSE(parser->results().apps.empty());
    EXPECT_EQ(parser->results().daystart_elapsed_days, 456);
  }
  {
    // Parse a no-update response.
    EXPECT_TRUE(parser->Parse(kJSONNoUpdate)) << parser->errors();
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    EXPECT_FALSE(parser->results().apps.empty());
    const auto* first_result = &parser->results().apps[0];
    EXPECT_STREQ("noupdate", first_result->status.c_str());
    EXPECT_EQ(first_result->app_id, "12345");
    EXPECT_FALSE(first_result->nextversion.IsValid());
  }
  {
    // Parse xml with one error and one success <app> tag.
    EXPECT_TRUE(parser->Parse(kJSONTwoAppsOneError));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    ASSERT_EQ(2u, parser->results().apps.size());
    const auto* first_result = &parser->results().apps[0];
    EXPECT_EQ(first_result->app_id, "aaaaaaaa");
    EXPECT_STREQ("error-unknownApplication", first_result->status.c_str());
    EXPECT_FALSE(first_result->nextversion.IsValid());
    const auto* second_result = &parser->results().apps[1];
    EXPECT_EQ(second_result->app_id, "bbbbbbbb");
    EXPECT_STREQ("ok", second_result->status.c_str());
    EXPECT_EQ(base::Version("1.2.3.4"), second_result->nextversion);
  }
  {
    // Parse xml with two apps setting the cohort info.
    EXPECT_TRUE(parser->Parse(kJSONTwoAppsSetCohort));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    ASSERT_EQ(2u, parser->results().apps.size());
    const auto* first_result = &parser->results().apps[0];
    EXPECT_EQ(first_result->app_id, "aaaaaaaa");
    EXPECT_EQ(first_result->cohort.value(), "1:2q3/");
    EXPECT_FALSE(first_result->cohort_name.has_value());
    EXPECT_FALSE(first_result->cohort_hint.has_value());
    const auto* second_result = &parser->results().apps[1];
    EXPECT_EQ(second_result->app_id, "bbbbbbbb");
    EXPECT_EQ(second_result->cohort, "1:33z@0.33");
    EXPECT_EQ(second_result->cohort_name, "cname");
    EXPECT_FALSE(first_result->cohort_hint.has_value());
  }
  {
    EXPECT_TRUE(parser->Parse(kJSONUpdateCheckStatusOkWithRunAction));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    EXPECT_FALSE(parser->results().apps.empty());
    const auto* first_result = &parser->results().apps[0];
    EXPECT_STREQ("ok", first_result->status.c_str());
    EXPECT_EQ(first_result->app_id, "12345");
    EXPECT_EQ(first_result->pipelines[0].operations[2].path, "file.exe");
  }
  {
    EXPECT_TRUE(parser->Parse(kJSONAppsStatusError));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    EXPECT_EQ(9u, parser->results().apps.size());
    size_t index = 0;
    for (const std::string expected_status : {
             "error-unknownApplication",
             "restricted",
             "error-invalidAppId",
             "error-osnotsupported",
             "error-hwnotsupported",
             "error-hash",
             "error-unsupportedprotocol",
             "error-internal",
             "foobar",
         }) {
      const auto* result = &parser->results().apps[index];
      EXPECT_EQ(result->app_id, std::string(8, 'a' + index++));
      EXPECT_EQ(expected_status, result->status);
      EXPECT_FALSE(result->nextversion.IsValid());
    }
  }
  {
    EXPECT_TRUE(parser->Parse(kJSONManifestRun));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    ASSERT_EQ(1u, parser->results().apps.size());
    const auto& result = parser->results().apps[0];
    EXPECT_STREQ("UpdaterSetup.exe",
                 result.pipelines[0].operations[1].path.c_str());
    EXPECT_STREQ("--arg1 --arg2",
                 result.pipelines[0].operations[1].arguments.c_str());

    ASSERT_EQ(1u, result.data.size());
    EXPECT_STREQ("foobar_install_data_index",
                 result.data[0].install_data_index.c_str());
    EXPECT_STREQ("sampledata", result.data[0].text.c_str());
  }
}

TEST(UpdateClientProtocolParserJSONTest, ParseAttrs) {
  const auto parser = std::make_unique<ProtocolParserJSON>();
  {  // No custom attrs in kJSONManifestRun
    EXPECT_TRUE(parser->Parse(kJSONManifestRun));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    ASSERT_EQ(1u, parser->results().apps.size());
    const auto& result = parser->results().apps[0];
    EXPECT_EQ(0u, result.custom_attributes.size());
  }
  {  // Two custom attrs in kJSONCustomAttributes
    EXPECT_TRUE(parser->Parse(kJSONCustomAttributes));
    EXPECT_TRUE(parser->errors().empty()) << parser->errors();
    ASSERT_EQ(1u, parser->results().apps.size());
    const auto& result = parser->results().apps[0];
    ASSERT_EQ(2u, result.custom_attributes.size());
    EXPECT_EQ("example_value1", result.custom_attributes.at("_example1"));
    EXPECT_EQ("example_value2", result.custom_attributes.at("_example2"));
  }
}

}  // namespace update_client
