// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_NET_RESOURCE_PROVIDER_H_
#define CHROME_COMMON_NET_NET_RESOURCE_PROVIDER_H_

#include "base/memory/ref_counted_memory.h"

// This is called indirectly by the network layer to access resources.
scoped_refptr<base::RefCountedMemory> ChromeNetResourceProvider(int key);

#endif  // CHROME_COMMON_NET_NET_RESOURCE_PROVIDER_H_
