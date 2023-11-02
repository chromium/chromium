// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <objc/runtime.h>

#import "base/apple/scoped_nsobject.h"
#import "components/crash/core/common/objc_zombie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface ZombieCxxDestructTest : NSObject
{
  base::apple::scoped_nsobject<id> _aRef;
}
- (instancetype)initWith:(id)anObject;
@end

@implementation ZombieCxxDestructTest
- (instancetype)initWith:(id)anObject {
  self = [super init];
  if (self) {
    _aRef.reset([anObject retain]);
  }
  return self;
}
@end

@interface ZombieAssociatedObjectTest : NSObject
- (instancetype)initWithAssociatedObject:(id)anObject;
@end

@implementation ZombieAssociatedObjectTest

- (instancetype)initWithAssociatedObject:(id)anObject {
  if ((self = [super init])) {
    // The address of the variable itself is the unique key, the
    // contents don't matter.
    static char kAssociatedObjectKey = 'x';
    objc_setAssociatedObject(
        self, &kAssociatedObjectKey, anObject, OBJC_ASSOCIATION_RETAIN);
  }
  return self;
}

@end

namespace {

// Verify that the C++ destructors run when the last reference to the
// object is released.
// NOTE(shess): To test the negative, comment out the |g_objectDestruct()|
// call in |ZombieDealloc()|.
TEST(ObjcZombieTest, CxxDestructors) {
  base::apple::scoped_nsobject<id> anObject([[NSObject alloc] init]);
  EXPECT_EQ(1u, [anObject.get() retainCount]);

  ASSERT_TRUE(ObjcEvilDoers::ZombieEnable(YES, 100));

  base::apple::scoped_nsobject<ZombieCxxDestructTest> soonInfected(
      [[ZombieCxxDestructTest alloc] initWith:anObject.get()]);
  EXPECT_EQ(2u, [anObject.get() retainCount]);

  // When |soonInfected| becomes a zombie, the C++ destructors should
  // run and release a reference to |anObject|.
  soonInfected.reset();
  EXPECT_EQ(1u, [anObject.get() retainCount]);

  // The local reference should remain (C++ destructors aren't re-run).
  ObjcEvilDoers::ZombieDisable();
  EXPECT_EQ(1u, [anObject.get() retainCount]);
}

// Verify that the associated objects are released when the object is
// released.
TEST(ObjcZombieTest, AssociatedObjectsReleased) {
  base::apple::scoped_nsobject<id> anObject([[NSObject alloc] init]);
  EXPECT_EQ(1u, [anObject.get() retainCount]);

  ASSERT_TRUE(ObjcEvilDoers::ZombieEnable(YES, 100));

  base::apple::scoped_nsobject<ZombieAssociatedObjectTest> soonInfected(
      [[ZombieAssociatedObjectTest alloc]
          initWithAssociatedObject:anObject.get()]);
  EXPECT_EQ(2u, [anObject.get() retainCount]);

  // When |soonInfected| becomes a zombie, the associated object
  // should be released.
  soonInfected.reset();
  EXPECT_EQ(1u, [anObject.get() retainCount]);

  // The local reference should remain (associated objects not re-released).
  ObjcEvilDoers::ZombieDisable();
  EXPECT_EQ(1u, [anObject.get() retainCount]);
}

}  // namespace
