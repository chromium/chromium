// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_OBJC_ZOMBIE_H_
#define COMPONENTS_CRASH_CORE_COMMON_OBJC_ZOMBIE_H_

#include <stddef.h>

#include "build/build_config.h"
#include "components/crash/core/common/crash_export.h"

// You should think twice every single time you use anything from this
// namespace.
namespace ObjcEvilDoers {

// Enable zombie object debugging. This implements a variant of Apple's
// NSZombieEnabled which can help expose use-after-free errors where messages
// are sent to freed Objective-C objects in production builds.
//
// Returns NO if it fails to enable.
//
// When |zombieAllObjects| is YES, all objects inheriting from
// NSObject become zombies on -dealloc.  If NO, -shouldBecomeCrZombie
// is queried to determine whether to make the object a zombie.
//
// |zombieCount| controls how many zombies to store before freeing the
// oldest.  Set to 0 to free objects immediately after making them
// zombies.
bool CRASH_EXPORT ZombieEnable(bool zombieAllObjects, size_t zombieCount);

// Disable zombies.
void CRASH_EXPORT ZombieDisable();

}  // namespace ObjcEvilDoers

#if BUILDFLAG(IS_APPLE)
#if defined(__OBJC__)

#import <Foundation/Foundation.h>

@interface NSObject (CrZombie)
- (BOOL)shouldBecomeCrZombie;
@end

#endif  // __OBJC__
#endif  // BUILDFLAG(IS_APPLE)

#endif  // COMPONENTS_CRASH_CORE_COMMON_OBJC_ZOMBIE_H_
