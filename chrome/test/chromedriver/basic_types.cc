// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/basic_types.h"

WebPoint::WebPoint() : x(0), y(0) {}

WebPoint::WebPoint(double x, double y) : x(x), y(y) {}

WebPoint::~WebPoint() {}

void WebPoint::Offset(double x_, double y_) {
  x += x_;
  y += y_;
}

WebSize::WebSize() : width(0), height(0) {}

WebSize::WebSize(double width, double height) : width(width), height(height) {}

WebSize::~WebSize() {}

WebRect::WebRect() : origin(0, 0), size(0, 0) {}

WebRect::WebRect(double x, double y, double width, double height)
    : origin(x, y), size(width, height) {}

WebRect::WebRect(const WebPoint& origin, const WebSize& size)
    : origin(origin), size(size) {}

WebRect::~WebRect() {}

double WebRect::X() const {
  return origin.x;
}

double WebRect::Y() const {
  return origin.y;
}

double WebRect::Width() const {
  return size.width;
}

double WebRect::Height() const {
  return size.height;
}
