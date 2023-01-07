// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_TEST_START_CRONET_H_
#define COMPONENTS_CRONET_IOS_TEST_START_CRONET_H_

namespace cronet {

// Starts Cronet, or restarts if Cronet is already running.  Will have Cronet
// point test.example.com" to "localhost:|port|".
void StartCronet(int port);

}  // namespace cronet

#endif  // COMPONENTS_CRONET_IOS_TEST_START_CRONET_H_
