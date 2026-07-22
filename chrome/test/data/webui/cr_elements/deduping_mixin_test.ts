// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dedupingMixin as dedupingMixinLit} from 'chrome://resources/lit/v3_0/lit.rollup.js';
// <if expr="not is_android">
import {dedupingMixin as dedupingMixinPolymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// </if>
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

type Constructor<T> = new (...args: any[]) => T;

class BaseElement extends HTMLElement {
  connectedCallback() {}
}

const versions = [
  {name: 'Lit', mixin: dedupingMixinLit},
  // <if expr="not is_android">
  {name: 'Polymer', mixin: dedupingMixinPolymer},
  // </if>
];

versions.forEach(({name, mixin}) => {
  suite(`dedupingMixin_${name}`, function() {
    test('DedupesDirectApplication', function() {
      let mixinCalls = 0;

      const DummyMixin =
          mixin(<T extends Constructor<BaseElement>>(superClass: T): T => {
            mixinCalls++;
            class DummyMixin extends superClass {}
            return DummyMixin;
          });

      const Subclass1 = DummyMixin(BaseElement);
      assertEquals(1, mixinCalls);

      // Applying to the same base class again returns the same class.
      const Subclass2 = DummyMixin(BaseElement);
      assertEquals(1, mixinCalls);
      assertEquals(Subclass1, Subclass2);

      // Applying to a subclass that already has the mixin returns that subclass
      // directly.
      const Subclass3 = DummyMixin(Subclass1);
      assertEquals(1, mixinCalls);
      assertEquals(Subclass1, Subclass3);
    });

    test('DedupesTransitiveApplication', function() {
      let mixinACalls = 0;
      let mixinBCalls = 0;

      const MixinA =
          mixin(<T extends Constructor<BaseElement>>(superClass: T): T => {
            mixinACalls++;
            class MixinA extends superClass {}
            return MixinA;
          });

      const MixinB =
          mixin(<T extends Constructor<BaseElement>>(superClass: T): T => {
            mixinBCalls++;
            const superClassBase = MixinA(superClass);
            class MixinB extends superClassBase {}
            return MixinB;
          });

      // When MixinB is applied to a class that already has MixinA, MixinA is
      // not re-applied.
      const SubclassA = MixinA(BaseElement);
      assertEquals(1, mixinACalls);

      const SubclassBA = MixinB(SubclassA);
      assertEquals(1, mixinACalls);
      assertEquals(1, mixinBCalls);
      assertTrue(!!SubclassBA);

      // Directly applying MixinB to DummyTestElement applies MixinA and MixinB.
      class DummyTestElement extends BaseElement {
        static get is() {
          return `dummy-test-${name.toLowerCase()}`;
        }
      }
      customElements.define(DummyTestElement.is, DummyTestElement);

      const SubclassB = MixinB(DummyTestElement);
      assertEquals(2, mixinACalls);
      assertEquals(2, mixinBCalls);

      // Applying MixinA to a class that already has MixinB (which includes
      // MixinA) returns the class unchanged.
      const SubclassAB = MixinA(SubclassB);
      assertEquals(2, mixinACalls);
      assertEquals(SubclassB, SubclassAB);
    });

    test('LifecycleCallbacksExecutedOnce', function() {
      let connectedCallbackCalls = 0;

      const MixinA =
          mixin(<T extends Constructor<BaseElement>>(superClass: T): T => {
            class MixinA extends superClass {
              override connectedCallback() {
                super.connectedCallback();
                connectedCallbackCalls++;
              }
            }
            return MixinA;
          });

      const MixinB =
          mixin(<T extends Constructor<BaseElement>>(superClass: T): T => {
            const superClassBase = MixinA(superClass);
            class MixinB extends superClassBase {}
            return MixinB;
          });

      const TestDedupingElementBase = MixinB(MixinA(BaseElement));

      class TestDedupingElement extends TestDedupingElementBase {
        static get is() {
          return `test-deduping-${name.toLowerCase()}`;
        }
      }
      customElements.define(TestDedupingElement.is, TestDedupingElement);

      const element = document.createElement(TestDedupingElement.is);
      document.body.appendChild(element);

      assertEquals(1, connectedCallbackCalls);
    });
  });
});
