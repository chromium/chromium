// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/variations/net/variations_flags.h"
#include "components/variations/variations.mojom.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace variations {
namespace {

// Returns a ResourceRequest created from the given values.
network::ResourceRequest CreateResourceRequest(
    const std::string& request_initiator_url,
    bool is_outermost_main_frame,
    bool has_trusted_params,
    const std::string& isolation_info_top_frame_origin_url,
    const std::string& isolation_info_frame_origin_url) {
  network::ResourceRequest request;
  if (request_initiator_url.empty())
    return request;

  request.request_initiator = url::Origin::Create(GURL(request_initiator_url));
  request.is_outermost_main_frame = is_outermost_main_frame;
  if (!has_trusted_params)
    return request;

  request.trusted_params = network::ResourceRequest::TrustedParams();
  if (isolation_info_top_frame_origin_url.empty())
    return request;

  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther,
      url::Origin::Create(GURL(isolation_info_top_frame_origin_url)),
      url::Origin::Create(GURL(isolation_info_frame_origin_url)),
      net::SiteForCookies());
  return request;
}

// Wraps AppendVariationsHeaderWithCustomValue().
void AppendVariationsHeader(const GURL& destination,
                            Owner owner,
                            network::ResourceRequest* request) {
  base::flat_map<variations::mojom::GoogleWebVisibility, std::string> headers =
      {{variations::mojom::GoogleWebVisibility::FIRST_PARTY, "abc123"},
       {variations::mojom::GoogleWebVisibility::ANY, "xyz456"}};

  AppendVariationsHeaderWithCustomValue(
      destination, InIncognito::kNo,
      variations::mojom::VariationsHeaders::New(headers), owner, request);
}

}  // namespace

TEST(VariationsHttpHeadersTest, ShouldAppendVariationsHeader) {
  struct {
    const char* url;
    bool should_append_headers;
  } cases[] = {
      {"http://google.com", false},
      {"https://google.com", true},
      {"http://www.google.com", false},
      {"https://www.google.com", true},
      {"http://m.google.com", false},
      {"https://m.google.com", true},
      {"http://google.ca", false},
      {"https://google.ca", true},
      {"http://google.co.uk", false},
      {"https://google.co.uk", true},
      {"http://google.co.uk:8080/", false},
      {"https://google.co.uk:8080/", true},
      {"http://www.google.co.uk:8080/", false},
      {"https://www.google.co.uk:8080/", true},
      {"https://google", false},

      {"http://youtube.com", false},
      {"https://youtube.com", true},
      {"http://www.youtube.com", false},
      {"https://www.youtube.com", true},
      {"http://www.youtube.ca", false},
      {"https://www.youtube.ca", true},
      {"http://www.youtube.co.uk:8080/", false},
      {"https://www.youtube.co.uk:8080/", true},
      {"https://youtube", false},

      {"https://www.yahoo.com", false},

      {"http://ad.doubleclick.net", false},
      {"https://ad.doubleclick.net", true},
      {"https://a.b.c.doubleclick.net", true},
      {"https://a.b.c.doubleclick.net:8081", true},
      {"http://www.doubleclick.com", false},
      {"https://www.doubleclick.com", true},
      {"https://www.doubleclick.org", false},
      {"http://www.doubleclick.net.com", false},
      {"https://www.doubleclick.net.com", false},

      {"http://ad.googlesyndication.com", false},
      {"https://ad.googlesyndication.com", true},
      {"https://a.b.c.googlesyndication.com", true},
      {"https://a.b.c.googlesyndication.com:8080", true},
      {"http://www.doubleclick.edu", false},
      {"http://www.googlesyndication.com.edu", false},
      {"https://www.googlesyndication.com.com", false},

      {"http://www.googleadservices.com", false},
      {"https://www.googleadservices.com", true},
      {"http://www.googleadservices.com:8080", false},
      {"https://www.googleadservices.com:8080", true},
      {"https://www.internal.googleadservices.com", true},
      {"https://www2.googleadservices.com", true},
      {"https://www.googleadservices.org", false},
      {"https://www.googleadservices.com.co.uk", false},

      {"http://WWW.ANDROID.COM", false},
      {"https://WWW.ANDROID.COM", true},
      {"http://www.android.com", false},
      {"https://www.android.com", true},
      {"http://www.doubleclick.com", false},
      {"https://www.doubleclick.com", true},
      {"http://www.doubleclick.net", false},
      {"https://www.doubleclick.net", true},
      {"http://www.ggpht.com", false},
      {"https://www.ggpht.com", true},
      {"http://www.googleadservices.com", false},
      {"https://www.googleadservices.com", true},
      {"http://www.googleapis.com", false},
      {"https://www.googleapis.com", true},
      {"http://www.googlesyndication.com", false},
      {"https://www.googlesyndication.com", true},
      {"http://www.googleusercontent.com", false},
      {"https://www.googleusercontent.com", true},
      {"http://www.googlevideo.com", false},
      {"https://www.googlevideo.com", true},
      {"http://ssl.gstatic.com", false},
      {"https://ssl.gstatic.com", true},
      {"http://www.gstatic.com", false},
      {"https://www.gstatic.com", true},
      {"http://www.ytimg.com", false},
      {"https://www.ytimg.com", true},
      {"https://wwwytimg.com", false},
      {"https://ytimg.com", false},

      {"https://www.android.org", false},
      {"https://www.doubleclick.org", false},
      {"http://www.doubleclick.net", false},
      {"https://www.doubleclick.net", true},
      {"https://www.ggpht.org", false},
      {"https://www.googleadservices.org", false},
      {"https://www.googleapis.org", false},
      {"https://www.googlesyndication.org", false},
      {"https://www.googleusercontent.org", false},
      {"https://www.googlevideo.org", false},
      {"https://ssl.gstatic.org", false},
      {"https://www.gstatic.org", false},
      {"https://www.ytimg.org", false},

      {"http://a.b.android.com", false},
      {"https://a.b.android.com", true},
      {"http://a.b.doubleclick.com", false},
      {"https://a.b.doubleclick.com", true},
      {"http://a.b.doubleclick.net", false},
      {"https://a.b.doubleclick.net", true},
      {"http://a.b.ggpht.com", false},
      {"https://a.b.ggpht.com", true},
      {"http://a.b.googleadservices.com", false},
      {"https://a.b.googleadservices.com", true},
      {"http://a.b.googleapis.com", false},
      {"https://a.b.googleapis.com", true},
      {"http://a.b.googlesyndication.com", false},
      {"https://a.b.googlesyndication.com", true},
      {"http://a.b.googleusercontent.com", false},
      {"https://a.b.googleusercontent.com", true},
      {"http://a.b.googlevideo.com", false},
      {"https://a.b.googlevideo.com", true},
      {"http://ssl.gstatic.com", false},
      {"https://ssl.gstatic.com", true},
      {"http://a.b.gstatic.com", false},
      {"https://a.b.gstatic.com", true},
      {"http://a.b.ytimg.com", false},
      {"https://a.b.ytimg.com", true},
      {"http://googleweblight.com", false},
      {"https://googleweblight.com", true},
      {"http://wwwgoogleweblight.com", false},
      {"https://www.googleweblight.com", false},
      {"https://a.b.googleweblight.com", false},

      {"http://a.b.litepages.googlezip.net", false},
      {"https://litepages.googlezip.net", false},
      {"https://a.litepages.googlezip.net", true},
      {"https://a.b.litepages.googlezip.net", true},

      {"https://127.0.0.1", false},
      {"http://127.0.0.1", false},
      {"https://127.0.0.1:12345", false},
      {"http://127.0.0.1:12345", false},
  };

  for (const auto& c : cases) {
    const GURL url(c.url);
    EXPECT_EQ(c.should_append_headers,
              ShouldAppendVariationsHeaderForTesting(url, "Append"))
        << url;
  }
}

#if BUILDFLAG(IS_IOS)
TEST(VariationsHttpHeadersTest, ShouldAppendVariationsHeaderLocalhost) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      variations::kAppendVariationsHeadersToLocalhostForTesting);
  struct {
    const char* url;
    bool should_append_headers;
  } cases[] = {
      {"https://127.0.0.1", true},
      {"http://127.0.0.1", true},
      {"https://127.0.0.1:12345", true},
      {"http://127.0.0.1:12345", true},
  };

  for (const auto& c : cases) {
    const GURL url(c.url);
    EXPECT_EQ(c.should_append_headers,
              ShouldAppendVariationsHeaderForTesting(url, "Append"))
        << url;
  }
}
#endif  // BUILDFLAG(IS_IOS)

struct PopulateRequestContextHistogramData {
  const char* request_initiator_url;
  bool is_outermost_main_frame;
  bool has_trusted_params;
  const char* isolation_info_top_frame_origin_url;
  const char* isolation_info_frame_origin_url;
  bool is_top_level_google_owned;
  int bucket;
  const char* name;
};

class PopulateRequestContextHistogramTest
    : public testing::TestWithParam<PopulateRequestContextHistogramData> {
 public:
  static const PopulateRequestContextHistogramData kCases[];

  // Required to use ObserverListThreadSafe::AddObserver() from:
  //    base::FieldTrialList::AddObserver
  //    variations::VariationsIdsProvider::InitVariationIDsCacheIfNeeded
  //    variations::VariationsIdsProvider::GetClientDataHeader
  //    variations::VariationsHeaderHelper::VariationsHeaderHelper
  //    variations::AppendVariationsHeaderUnknownSignedIn
  base::test::SingleThreadTaskEnvironment task_environment_;
};

const PopulateRequestContextHistogramData
    PopulateRequestContextHistogramTest::kCases[] = {
        {"", false, false, "", "", false, 0, "kBrowserInitiated"},
        {"chrome://newtab/", false, false, "", "", false, 1,
         "kInternalChromePageInitiated"},
        {"chrome-search://most-visited/title.html", false, false, "", "", false,
         1, "kInternalChromePageInitiated"},
        {"https://www.youtube.com/", true, false, "", "", false, 2,
         "kGooglePageInitiated"},
        {"https://docs.google.com/", false, true, "https://drive.google.com/",
         "https://docs.google.com/", false, 3,
         "kGoogleSubFrameOnGooglePageInitiated with TrustedParams"},
        {"https://docs.google.com/", false, false, "", "", true, 3,
         "kGoogleSubFrameOnGooglePageInitiated without TrustedParams"},
        {"https://www.un.org/", false, false, "", "", false, 4,
         "kNonGooglePageInitiated"},
        // Bucket 5, kNoTrustedParams, is deprecated.
        {"https://foo.google.com/", false, true, "", "", false, 6,
         "kNoIsolationInfo"},
        {"https://foo.gstatic.com/", false, true, "https://www.lexico.com/", "",
         false, 7,
         "kGoogleSubFrameOnNonGooglePageInitiated with TrustedParams"},
        {"https://foo.gstatic.com/", false, false, "", "", false, 7,
         "kGoogleSubFrameOnNonGooglePageInitiated without TrustedParams"},
        // Bucket 8, kNonGooglePageInitiatedFromFrameOrigin, is deprecated.
};

TEST(VariationsHttpHeadersTest, PopulateUrlValidationResultHistograms) {
  const GURL invalid_url("invalid");
  const GURL not_google("https://heavnlydonuts.com/");
  const GURL should_append("https://youtube.com");
  const GURL wrong_scheme("ftp://foo.com/");
  const GURL google_not_https("http://google.com/");

  const std::string append = "Append";
  const std::string remove = "Remove";
  base::HistogramTester tester;

  ASSERT_FALSE(ShouldAppendVariationsHeaderForTesting(invalid_url, append));
  ASSERT_FALSE(ShouldAppendVariationsHeaderForTesting(not_google, append));
  ASSERT_TRUE(ShouldAppendVariationsHeaderForTesting(should_append, append));

  ASSERT_FALSE(ShouldAppendVariationsHeaderForTesting(wrong_scheme, remove));
  ASSERT_FALSE(
      ShouldAppendVariationsHeaderForTesting(google_not_https, remove));

  // Verify that the Append suffixed histogram has a sample corresponding to
  // the validation result for the three URLs validated for appending.
  const std::string append_histogram =
      "Variations.Headers.URLValidationResult.Append";
  tester.ExpectTotalCount(append_histogram, 3);
  EXPECT_THAT(tester.GetAllSamples(append_histogram),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(2, 1),
                                   base::Bucket(3, 1)));

  // Verify that the Remove suffixed histogram has a sample corresponding to
  // the validation result for the two URLs validated for removal.
  const std::string remove_histogram =
      "Variations.Headers.URLValidationResult.Remove";
  tester.ExpectTotalCount(remove_histogram, 2);
  EXPECT_THAT(tester.GetAllSamples(remove_histogram),
              testing::ElementsAre(base::Bucket(4, 1), base::Bucket(5, 1)));
}

TEST(VariationsHttpHeadersTest, PopulateDomainOwnerHistogram) {
  const GURL destination("https://fonts.googleapis.com/foo");
  network::ResourceRequest request = CreateResourceRequest(
      /*request_initiator_url=*/"https://docs.google.com/",
      /*is_outermost_main_frame=*/false,
      /*has_trusted_params=*/false,
      /*isolation_info_top_frame_origin_url=*/"",
      /*isolation_info_frame_origin_url=*/"");

  base::HistogramTester tester;
  AppendVariationsHeader(destination, Owner::kUnknownFromRenderer, &request);
  AppendVariationsHeader(destination, Owner::kUnknown, &request);
  AppendVariationsHeader(destination, Owner::kNotGoogle, &request);
  AppendVariationsHeader(destination, Owner::kGoogle, &request);
  EXPECT_THAT(tester.GetAllSamples("Variations.Headers.DomainOwner"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1),
                                   base::Bucket(2, 1), base::Bucket(3, 1)));
}

INSTANTIATE_TEST_SUITE_P(
    VariationsHttpHeadersTest,
    PopulateRequestContextHistogramTest,
    testing::ValuesIn(PopulateRequestContextHistogramTest::kCases));

TEST_P(PopulateRequestContextHistogramTest, PopulateRequestContextHistogram) {
  PopulateRequestContextHistogramData data = GetParam();
  SCOPED_TRACE(data.name);

  network::ResourceRequest request = CreateResourceRequest(
      data.request_initiator_url, data.is_outermost_main_frame,
      data.has_trusted_params, data.isolation_info_top_frame_origin_url,
      data.isolation_info_frame_origin_url);

  base::HistogramTester tester;
  AppendVariationsHeader(
      GURL("https://foo.google.com"),
      data.is_top_level_google_owned ? Owner::kGoogle : Owner::kNotGoogle,
      &request);

  // Verify that the histogram has a single sample corresponding to the request
  // context category.
  const std::string histogram = "Variations.Headers.RequestContextCategory";
  tester.ExpectUniqueSample(histogram, data.bucket, 1);
}

}  // namespace variations
