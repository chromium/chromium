// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_BASIC_TYPES_H_
#define CHROME_TEST_CHROMEDRIVER_BASIC_TYPES_H_

struct WebPoint {
  WebPoint();
  WebPoint(double x, double y);
  ~WebPoint();

  void Offset(double x_, double y_);

  double x;
  double y;
};

struct WebSize {
  WebSize();
  WebSize(double width, double height);
  ~WebSize();

  double width;
  double height;
};

struct WebRect {
  WebRect();
  WebRect(double x, double y, double width, double height);
  WebRect(const WebPoint& origin, const WebSize& size);
  ~WebRect();

  double X() const;
  double Y() const;
  double Width() const;
  double Height() const;

  WebPoint origin;
  WebSize size;
};

#endif  // CHROME_TEST_CHROMEDRIVER_BASIC_TYPES_H_
