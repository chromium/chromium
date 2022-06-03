// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser_json.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

const char* kJSONValid = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"},
                    {"codebasediff":"http://diff.example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }})";

const char* kJSONHash = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx",
                              "hash_sha256":"1234",
                              "hashdiff_sha256":"5678"}]}}
     }
    }
   ]
  }})";

const char* kJSONInvalidSizes = R"()]}'
 {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "packages":{"package":[{"name":"1","size":1234},
                             {"name":"2","size":9007199254740991},
                             {"name":"3","size":-1234},
                             {"name":"4"},
                             {"name":"5","size":"-a"},
                             {"name":"6","size":-123467890123456789},
                             {"name":"7","size":123467890123456789},
                             {"name":"8","sizediff":1234},
                             {"name":"9","sizediff":9007199254740991},
                             {"name":"10","sizediff":-1234},
                             {"name":"11","sizediff":"-a"},
                             {"name":"12","sizediff":-123467890123456789},
                             {"name":"13","sizediff":123467890123456789}]}}
      }
     }
    ]
   }})";

const char* kJSONInvalidMissingCodebase = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebasediff":"http://diff.example.com"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "packages":{"package":[{"namediff":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }})";

const char* kJSONInvalidMissingManifest = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://localhost/download/"}]}
     }
    }
   ]
  }})";

const char* kJSONMissingAppId = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://localhost/download/"}]}
     }
    }
   ]
  }})";

const char* kJSONInvalidCodebase = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"example.com/extension_1.2.3.4.crx",
                     "version":"1.2.3.4"}]}
     }
    }
   ]
  }})";

const char* kJSONMissingVersion = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://localhost/download/"}]},
     "manifest":{
      "packages":{"package":[{"name":"jebgalgnebhfojomionfpkfelancnnkf.crx"}]}}
     }
    }
   ]
  }})";

const char* kJSONInvalidVersion = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://localhost/download/"}]},
     "manifest":{
      "version":"1.2.3.a",
      "packages":{"package":[{"name":"jebgalgnebhfojomionfpkfelancnnkf.crx"}]}}
     }
    }
   ]
  }})";

// Includes a <daystart> tag.
const char* kJSONWithDaystart = R"()]}'
  {"response":{
   "protocol":"3.1",
   "daystart":{"elapsed_seconds":456},
   "app":[
    {"appid":"12345",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"},
                    {"codebasediff":"http://diff.example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }})";

// Indicates no updates available.
const char* kJSONNoUpdate = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
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
   "protocol":"3.1",
   "daystart":{"elapsed_seconds":456},
   "app":[
    {"appid":"aaaaaaaa",
     "status":"error-unknownApplication",
     "updatecheck":{"status":"error-internal"}
    },
    {"appid":"bbbbbbbb",
     "status":"ok",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }})";

// Includes two <app> tags, both of which set the cohort.
const char* kJSONTwoAppsSetCohort = R"()]}'
  {"response":{
   "protocol":"3.1",
   "daystart":{"elapsed_seconds":456},
   "app":[
    {"appid":"aaaaaaaa",
     "cohort":"1:2q3/",
     "updatecheck":{"status":"noupdate"}
    },
    {"appid":"bbbbbbbb",
     "cohort":"1:33z@0.33",
     "cohortname":"cname",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }})";

// Includes a run action for an update check with status='ok'.
const char* kJSONUpdateCheckStatusOkWithRunAction = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "updatecheck":{
     "status":"ok",
     "actions":{"action":[{"run":"this"}]},
     "urls":{"url":[{"codebase":"http://example.com/"},
                    {"codebasediff":"http://diff.example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }})";

// Includes a run action for an update check with status='noupdate'.
const char* kJSONUpdateCheckStatusNoUpdateWithRunAction = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "updatecheck":{
     "status":"noupdate",
     "actions":{"action":[{"run":"this"}]}
     }
    }
   ]
  }})";

// Includes a run action for an update check with status='error'.
const char* kJSONUpdateCheckStatusErrorWithRunAction = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "updatecheck":{
     "status":"error-osnotsupported",
     "actions":{"action":[{"run":"this"}]}
     }
    }
   ]
  }})";

// Includes four app objects with status different than 'ok'.
const char* kJSONAppsStatusError = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"aaaaaaaa",
     "status":"error-unknownApplication",
     "updatecheck":{"status":"error-internal"}
    },
    {"appid":"bbbbbbbb",
     "status":"restricted",
     "updatecheck":{"status":"error-internal"}
    },
    {"appid":"cccccccc",
     "status":"error-invalidAppId",
     "updatecheck":{"status":"error-internal"}
    },
    {"appid":"dddddddd",
     "status":"foobar",
     "updatecheck":{"status":"error-internal"}
    }
   ]
  }})";

// Includes a manifest |run| value for an update check with status='ok'.
const char* kJSONManifestRun = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
     "updatecheck":{
     "status":"ok",
     "urls":{"url":[{"codebase":"http://example.com/"},
                    {"codebasediff":"http://diff.example.com/"}]},
     "manifest":{
      "version":"1.2.3.4",
      "prodversionmin":"2.0.143.0",
      "run":"UpdaterSetup.exe",
      "arguments":"--arg1 --arg2",
      "packages":{"package":[{"name":"extension_1_2_3_4.crx"}]}}
     }
    }
   ]
  }})";

// Includes two custom response attributes in the update_check.
const char* kJSONCustomAttributes = R"()]}'
  {"response":{
   "protocol":"3.1",
   "app":[
    {"appid":"12345",
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
  EXPECT_TRUE(parser->results().list.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONInvalidCodebase));
  EXPECT_TRUE(parser->results().list.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONMissingVersion));
  EXPECT_TRUE(parser->results().list.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONInvalidVersion));
  EXPECT_TRUE(parser->results().list.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONInvalidMissingCodebase));
  EXPECT_TRUE(parser->results().list.empty());
  EXPECT_FALSE(parser->errors().empty());

  EXPECT_TRUE(parser->Parse(kJSONInvalidMissingManifest));
  EXPECT_TRUE(parser->results().list.empty());
  EXPECT_FALSE(parser->errors().empty());

  {
    // Parse some valid XML, and check that all params came out as expected.
    EXPECT_TRUE(parser->Parse(kJSONValid));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_EQ(1u, parser->results().list.size());
    const auto* first_result = &parser->results().list[0];
    EXPECT_STREQ("ok", first_result->status.c_str());
    EXPECT_EQ(1u, first_result->crx_urls.size());
    EXPECT_EQ(GURL("http://example.com/"), first_result->crx_urls[0]);
    EXPECT_EQ(GURL("http://diff.example.com/"), first_result->crx_diffurls[0]);
    EXPECT_EQ("1.2.3.4", first_result->manifest.version);
    EXPECT_EQ("2.0.143.0", first_result->manifest.browser_min_version);
    EXPECT_EQ(1u, first_result->manifest.packages.size());
    EXPECT_EQ("extension_1_2_3_4.crx", first_result->manifest.packages[0].name);
  }
  {
    // Parse xml with hash value.
    EXPECT_TRUE(parser->Parse(kJSONHash));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_FALSE(parser->results().list.empty());
    const auto* first_result = &parser->results().list[0];
    EXPECT_FALSE(first_result->manifest.packages.empty());
    EXPECT_EQ("1234", first_result->manifest.packages[0].hash_sha256);
    EXPECT_EQ("5678", first_result->manifest.packages[0].hashdiff_sha256);
  }
  {
    // Parse xml with package size value.
    EXPECT_TRUE(parser->Parse(kJSONInvalidSizes));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_FALSE(parser->results().list.empty());
    const auto* first_result = &parser->results().list[0];
    EXPECT_FALSE(first_result->manifest.packages.empty());
    EXPECT_EQ(1234, first_result->manifest.packages[0].size);
    EXPECT_EQ(9007199254740991, first_result->manifest.packages[1].size);
    EXPECT_EQ(0, first_result->manifest.packages[2].size);
    EXPECT_EQ(0, first_result->manifest.packages[3].size);
    EXPECT_EQ(0, first_result->manifest.packages[4].size);
    EXPECT_EQ(0, first_result->manifest.packages[5].size);
    EXPECT_EQ(0, first_result->manifest.packages[6].size);
    EXPECT_EQ(1234, first_result->manifest.packages[7].sizediff);
    EXPECT_EQ(9007199254740991, first_result->manifest.packages[8].sizediff);
    EXPECT_EQ(0, first_result->manifest.packages[9].sizediff);
    EXPECT_EQ(0, first_result->manifest.packages[10].sizediff);
    EXPECT_EQ(0, first_result->manifest.packages[11].sizediff);
    EXPECT_EQ(0, first_result->manifest.packages[12].sizediff);
  }
  {
    // Parse xml with a <daystart> element.
    EXPECT_TRUE(parser->Parse(kJSONWithDaystart));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_FALSE(parser->results().list.empty());
    EXPECT_EQ(parser->results().daystart_elapsed_seconds, 456);
  }
  {
    // Parse a no-update response.
    EXPECT_TRUE(parser->Parse(kJSONNoUpdate));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_FALSE(parser->results().list.empty());
    const auto* first_result = &parser->results().list[0];
    EXPECT_STREQ("noupdate", first_result->status.c_str());
    EXPECT_EQ(first_result->extension_id, "12345");
    EXPECT_EQ(first_result->manifest.version, "");
  }
  {
    // Parse xml with one error and one success <app> tag.
    EXPECT_TRUE(parser->Parse(kJSONTwoAppsOneError));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_EQ(2u, parser->results().list.size());
    const auto* first_result = &parser->results().list[0];
    EXPECT_EQ(first_result->extension_id, "aaaaaaaa");
    EXPECT_STREQ("error-unknownApplication", first_result->status.c_str());
    EXPECT_TRUE(first_result->manifest.version.empty());
    const auto* second_result = &parser->results().list[1];
    EXPECT_EQ(second_result->extension_id, "bbbbbbbb");
    EXPECT_STREQ("ok", second_result->status.c_str());
    EXPECT_EQ("1.2.3.4", second_result->manifest.version);
  }
  {
    // Parse xml with two apps setting the cohort info.
    EXPECT_TRUE(parser->Parse(kJSONTwoAppsSetCohort));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_EQ(2u, parser->results().list.size());
    const auto* first_result = &parser->results().list[0];
    EXPECT_EQ(first_result->extension_id, "aaaaaaaa");
    EXPECT_NE(first_result->cohort_attrs.find("cohort"),
              first_result->cohort_attrs.end());
    EXPECT_EQ(first_result->cohort_attrs.find("cohort")->second, "1:2q3/");
    EXPECT_EQ(first_result->cohort_attrs.find("cohortname"),
              first_result->cohort_attrs.end());
    EXPECT_EQ(first_result->cohort_attrs.find("cohorthint"),
              first_result->cohort_attrs.end());
    const auto* second_result = &parser->results().list[1];
    EXPECT_EQ(second_result->extension_id, "bbbbbbbb");
    EXPECT_NE(second_result->cohort_attrs.find("cohort"),
              second_result->cohort_attrs.end());
    EXPECT_EQ(second_result->cohort_attrs.find("cohort")->second, "1:33z@0.33");
    EXPECT_NE(second_result->cohort_attrs.find("cohortname"),
              second_result->cohort_attrs.end());
    EXPECT_EQ(second_result->cohort_attrs.find("cohortname")->second, "cname");
    EXPECT_EQ(second_result->cohort_attrs.find("cohorthint"),
              second_result->cohort_attrs.end());
  }
  {
    EXPECT_TRUE(parser->Parse(kJSONUpdateCheckStatusOkWithRunAction));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_FALSE(parser->results().list.empty());
    const auto* first_result = &parser->results().list[0];
    EXPECT_STREQ("ok", first_result->status.c_str());
    EXPECT_EQ(first_result->extension_id, "12345");
    EXPECT_STREQ("this", first_result->action_run.c_str());
  }
  {
    EXPECT_TRUE(parser->Parse(kJSONUpdateCheckStatusNoUpdateWithRunAction));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_FALSE(parser->results().list.empty());
    const auto* first_result = &parser->results().list[0];
    EXPECT_STREQ("noupdate", first_result->status.c_str());
    EXPECT_EQ(first_result->extension_id, "12345");
    EXPECT_STREQ("this", first_result->action_run.c_str());
  }
  {
    EXPECT_TRUE(parser->Parse(kJSONUpdateCheckStatusErrorWithRunAction));
    EXPECT_FALSE(parser->errors().empty());
    EXPECT_TRUE(parser->results().list.empty());
  }
  {
    EXPECT_TRUE(parser->Parse(kJSONAppsStatusError));
    EXPECT_STREQ("Unknown app status", parser->errors().c_str());
    EXPECT_EQ(3u, parser->results().list.size());
    const auto* first_result = &parser->results().list[0];
    EXPECT_EQ(first_result->extension_id, "aaaaaaaa");
    EXPECT_STREQ("error-unknownApplication", first_result->status.c_str());
    EXPECT_TRUE(first_result->manifest.version.empty());
    const auto* second_result = &parser->results().list[1];
    EXPECT_EQ(second_result->extension_id, "bbbbbbbb");
    EXPECT_STREQ("restricted", second_result->status.c_str());
    EXPECT_TRUE(second_result->manifest.version.empty());
    const auto* third_result = &parser->results().list[2];
    EXPECT_EQ(third_result->extension_id, "cccccccc");
    EXPECT_STREQ("error-invalidAppId", third_result->status.c_str());
    EXPECT_TRUE(third_result->manifest.version.empty());
  }
  {
    EXPECT_TRUE(parser->Parse(kJSONManifestRun));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_EQ(1u, parser->results().list.size());
    const auto& result = parser->results().list[0];
    EXPECT_STREQ("UpdaterSetup.exe", result.manifest.run.c_str());
    EXPECT_STREQ("--arg1 --arg2", result.manifest.arguments.c_str());
  }
}

TEST(UpdateClientProtocolParserJSONTest, ParseAttrs) {
  const auto parser = std::make_unique<ProtocolParserJSON>();
  {  // No custom attrs in kJSONManifestRun
    EXPECT_TRUE(parser->Parse(kJSONManifestRun));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_EQ(1u, parser->results().list.size());
    const auto& result = parser->results().list[0];
    EXPECT_EQ(0u, result.custom_attributes.size());
  }
  {  // Two custom attrs in kJSONCustomAttributes
    EXPECT_TRUE(parser->Parse(kJSONCustomAttributes));
    EXPECT_TRUE(parser->errors().empty());
    EXPECT_EQ(1u, parser->results().list.size());
    const auto& result = parser->results().list[0];
    EXPECT_EQ(2u, result.custom_attributes.size());
    EXPECT_EQ("example_value1", result.custom_attributes.at("_example1"));
    EXPECT_EQ("example_value2", result.custom_attributes.at("_example2"));
  }
}

}  // namespace update_client
