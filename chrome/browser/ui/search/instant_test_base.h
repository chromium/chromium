// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_BASE_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_BASE_H_

#include "net/test/embedded_test_server/embedded_test_server.h"

class GURL;
class Profile;

// This utility class is meant to be used in a "mix-in" fashion, giving the
// derived test class additional Instant-related functionality.
class InstantTestBase {
 public:
  InstantTestBase(const InstantTestBase&) = delete;
  InstantTestBase& operator=(const InstantTestBase&) = delete;

 protected:
  InstantTestBase();
  virtual ~InstantTestBase();

  void SetupInstant(Profile* profile,
                    const GURL& base_url,
                    const GURL& ntp_url);

  net::EmbeddedTestServer& https_test_server() { return https_test_server_; }

 private:
  // HTTPS Testing server, started on demand.
  net::EmbeddedTestServer https_test_server_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_TEST_BASE_H_
