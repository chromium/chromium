// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/link_header_util/link_header_util.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace link_header_util {

namespace {

void SplitLinkHeaderForTesting(const std::string& header,
                               std::vector<std::string>* values) {
  std::vector<StringIteratorPair> values_iterators = SplitLinkHeader(header);
  values->clear();
  for (const auto& pair : values_iterators)
    values->push_back(std::string(pair.first, pair.second));
}

bool ParseLinkHeaderValueForTesting(
    std::string value,
    std::string* url,
    std::unordered_map<std::string, std::optional<std::string>>* params) {
  return ParseLinkHeaderValue(value.begin(), value.end(), url, params);
}

TEST(LinkHeaderTest, SplitEmpty) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting("", &values);
  ASSERT_EQ(0u, values.size());
}

TEST(LinkHeaderTest, SplitSimple) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting("hello", &values);
  ASSERT_EQ(1u, values.size());
  EXPECT_EQ("hello", values[0]);

  SplitLinkHeaderForTesting("foo, bar", &values);
  ASSERT_EQ(2u, values.size());
  EXPECT_EQ("foo", values[0]);
  EXPECT_EQ("bar", values[1]);

  SplitLinkHeaderForTesting(" 1\t,\t2,3", &values);
  ASSERT_EQ(3u, values.size());
  EXPECT_EQ("1", values[0]);
  EXPECT_EQ("2", values[1]);
  EXPECT_EQ("3", values[2]);
}

TEST(LinkHeaderTest, SplitSkipsEmpty) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting(", foo, , \t, bar", &values);
  ASSERT_EQ(2u, values.size());
  EXPECT_EQ("foo", values[0]);
  EXPECT_EQ("bar", values[1]);
}

TEST(LinkHeaderTest, SplitQuotes) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting("\"foo,bar\", 'bar,foo', <hel,lo>", &values);
  ASSERT_EQ(4u, values.size());
  EXPECT_EQ("\"foo,bar\"", values[0]);
  EXPECT_EQ("'bar", values[1]);
  EXPECT_EQ("foo'", values[2]);
  EXPECT_EQ("<hel,lo>", values[3]);
}

TEST(LinkHeaderTest, SplitEscapedQuotes) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting("\"f\\\"oo,bar\", 'b\\'ar,foo', <hel\\>,lo>",
                            &values);
  ASSERT_EQ(5u, values.size());
  EXPECT_EQ("\"f\\\"oo,bar\"", values[0]);
  EXPECT_EQ("'b\\'ar", values[1]);
  EXPECT_EQ("foo'", values[2]);
  EXPECT_EQ("<hel\\>", values[3]);
  EXPECT_EQ("lo>", values[4]);
}

struct SimpleParseTestData {
  const char* link;
  bool valid;
  const char* url;
  const char* rel;
  const char* as;
};

void PrintTo(const SimpleParseTestData& test, std::ostream* os) {
  *os << ::testing::PrintToString(test.link);
}

class SimpleParseTest : public ::testing::TestWithParam<SimpleParseTestData> {};

TEST_P(SimpleParseTest, Simple) {
  const SimpleParseTestData test = GetParam();

  std::string url;
  std::unordered_map<std::string, std::optional<std::string>> params;
  EXPECT_EQ(test.valid,
            ParseLinkHeaderValueForTesting(test.link, &url, &params));
  if (test.valid) {
    EXPECT_EQ(test.url, url);
    EXPECT_EQ(test.rel, params["rel"].value_or(""));
    EXPECT_EQ(test.as, params["as"].value_or(""));
  }
}

// Test data mostly copied from blink::LinkHeaderTest. Expectations for a small
// subset of test cases are different though. Mostly because blink::LinkHeader
// factors in knowledge about semantics of Link headers (parameters that are
// required to have a value if they occur, some parameters are auto-lower-cased,
// headers with an "anchor" parameter are rejected by base::LinkHeader).
// The code this tests purely parses without actually interpreting the data, as
// it is expected that another layer on top will do more specific validations.
const SimpleParseTestData simple_parse_tests[] = {
    {"</images/cat.jpg>; rel=prefetch", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>;rel=prefetch", true, "/images/cat.jpg", "prefetch", ""},
    {"</images/cat.jpg>   ;rel=prefetch", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"< /images/cat.jpg>   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg >   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg wutwut>   ;   rel=prefetch", true,
     "/images/cat.jpg wutwut", "prefetch", ""},
    {"</images/cat.jpg wutwut  \t >   ;   rel=prefetch", true,
     "/images/cat.jpg wutwut", "prefetch", ""},
    {"</images/cat.jpg>; rel=prefetch   ", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>; Rel=prefetch   ", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>; Rel=PReFetCh   ", true, "/images/cat.jpg", "PReFetCh",
     ""},
    {"</images/cat.jpg>; rel=prefetch; rel=somethingelse", true,
     "/images/cat.jpg", "prefetch", ""},
    {"</images/cat.jpg>\t\t ; \trel=prefetch \t  ", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg>; rel= prefetch", true, "/images/cat.jpg", "prefetch",
     ""},
    {"<../images/cat.jpg?dog>; rel= prefetch", true, "../images/cat.jpg?dog",
     "prefetch", ""},
    {"</images/cat.jpg>; rel =prefetch", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>; rel pel=prefetch", false},
    {"< /images/cat.jpg>", true, "/images/cat.jpg", "", ""},
    {"</images/cat.jpg>; wut=sup; rel =prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg>; wut=sup ; rel =prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg>; wut=sup ; rel =prefetch  \t  ;", true,
     "/images/cat.jpg", "prefetch", ""},
    {"</images/cat.jpg> wut=sup ; rel =prefetch  \t  ;", false},
    {"<   /images/cat.jpg", false},
    {"<   http://wut.com/  sdfsdf ?sd>; rel=dns-prefetch", true,
     "http://wut.com/  sdfsdf ?sd", "dns-prefetch", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=dns-prefetch", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "dns-prefetch", ""},
    {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>; rel=prefetch", true,
     "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", ""},
    {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>;;;;; rel=prefetch", true,
     "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=image", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "preload", "image"},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=whatever", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "preload", "whatever"},
    {"</images/cat.jpg>; rel=prefetch;", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>; rel=prefetch    ;", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/ca,t.jpg>; rel=prefetch    ;", true, "/images/ca,t.jpg",
     "prefetch", ""},
    {"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE and "
     "backslash\"",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE \\\" and "
     "backslash: \\\"",
     false},
    {"<simple.css>; title=\"title with a DQUOTE \\\" and backslash: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\'title with a DQUOTE \\\' and backslash: \'; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\"title with a DQUOTE \\\" and ;backslash,: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\"title with a DQUOTE \' and ;backslash,: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\"\"; rel=stylesheet; ", true, "simple.css",
     "stylesheet", ""},
    {"<simple.css>; title=\"\"; rel=\"stylesheet\"; ", true, "simple.css",
     "stylesheet", ""},
    {"<simple.css>; rel=stylesheet; title=\"", false},
    {"<simple.css>; rel=stylesheet; title=\"\"", true, "simple.css",
     "stylesheet", ""},
    {"<simple.css>; rel=\"stylesheet\"; title=\"", false},
    {"<simple.css>; rel=\";style,sheet\"; title=\"", false},
    {"<simple.css>; rel=\"bla'sdf\"; title=\"", false},
    {"<simple.css>; rel=\"\"; title=\"\"", true, "simple.css", "", ""},
    {"<simple.css>; rel=''; title=\"\"", true, "simple.css", "''", ""},
    {"<simple.css>; rel=''; bla", true, "simple.css", "''", ""},
    {"<simple.css>; rel='prefetch", true, "simple.css", "'prefetch", ""},
    {"<simple.css>; rel=\"prefetch", false},
    {"<simple.css>; rel=\"", false},
    {"simple.css; rel=prefetch", false},
    {"<simple.css>; rel=prefetch; rel=foobar", true, "simple.css", "prefetch",
     ""},
};

INSTANTIATE_TEST_SUITE_P(LinkHeaderTest,
                         SimpleParseTest,
                         testing::ValuesIn(simple_parse_tests));

}  // namespace

}  // namespace link_header_util
