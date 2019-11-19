// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var binding = getInternalApi('platformKeysInternal');

exports.$set('selectClientCertificates', binding.selectClientCertificates);
exports.$set('sign', binding.sign);
exports.$set('getPublicKey', binding.getPublicKey);
