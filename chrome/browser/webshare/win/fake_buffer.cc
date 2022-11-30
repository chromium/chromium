// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_buffer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webshare {

FakeBuffer::FakeBuffer(UINT32 capacity) : data_(capacity) {}

FakeBuffer::~FakeBuffer() = default;

IFACEMETHODIMP FakeBuffer::get_Capacity(UINT32* value) {
  *value = data_.size();
  return S_OK;
}
IFACEMETHODIMP FakeBuffer::get_Length(UINT32* value) {
  *value = length_;
  return S_OK;
}
IFACEMETHODIMP FakeBuffer::put_Length(UINT32 value) {
  if (value > data_.size()) {
    ADD_FAILURE() << "put_Length called with a value (" << value
                  << ") greater than the capacity (" << data_.size() << ")";
    return E_INVALIDARG;
  }
  length_ = value;
  return S_OK;
}

IFACEMETHODIMP FakeBuffer::Buffer(byte** value) {
  *value = data_.data();
  return S_OK;
}

}  // namespace webshare
