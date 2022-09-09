// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_GEOPOSITION_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_GEOPOSITION_H_

struct Geoposition {
  double latitude;
  double longitude;
  double accuracy;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_GEOPOSITION_H_
