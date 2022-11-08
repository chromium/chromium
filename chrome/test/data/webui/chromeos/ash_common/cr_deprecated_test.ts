// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

suite('CrDeprecatedModuleAddSingletonGetterTest', function() {
  test('addSingletonGetter', function() {
    class Foo {}
    addSingletonGetter(Foo);

    type FooWithGetInstance =
        typeof Foo&{getInstance: () => Foo, instance_?: Foo | null};

    assertEquals(
        'function', typeof (Foo as FooWithGetInstance).getInstance,
        'Should add get instance function');

    const x = (Foo as FooWithGetInstance).getInstance();
    assertEquals('object', typeof x, 'Should successfully create an object');
    assertNotEquals(null, x, 'Created object should not be null');

    const y = (Foo as FooWithGetInstance).getInstance();
    assertEquals(x, y, 'Should return the same object');

    delete (Foo as FooWithGetInstance).instance_;

    const z = (Foo as FooWithGetInstance).getInstance();
    assertEquals('object', typeof z, 'Should work after clearing for testing');
    assertNotEquals(null, z, 'Created object should not be null');

    assertNotEquals(
        x, z, 'Should return a different object after clearing for testing');
  });
});
