// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {html as polymerHtml, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertNotReached, assertNull, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

type Constructor<T> = new (...args: any[]) => T;

interface LifecycleCallbackTrackerMixinInterface {
  lifecycleCallbacks: string[];
}

const LifecycleCallbackTrackerMixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<LifecycleCallbackTrackerMixinInterface> => {
      class LifecycleCallbackTrackerMixin extends superClass {
        lifecycleCallbacks: string[] = [];

        override connectedCallback() {
          this.lifecycleCallbacks.push('connectedCallback');
          super.connectedCallback();
        }

        override disconnectedCallback() {
          this.lifecycleCallbacks.push('disconnectedCallback');
          super.disconnectedCallback();
        }

        override performUpdate() {
          this.lifecycleCallbacks.push('performUpdate');
          super.performUpdate();
        }
      }

      return LifecycleCallbackTrackerMixin;
    };

const CrDummyLitElementBase = LifecycleCallbackTrackerMixin(CrLitElement);

interface CrDummyLitElement {
  $: {
    foo: HTMLElement,
    bar: HTMLElement,
  };
}

class CrDummyLitElement extends CrDummyLitElementBase {
  static get is() {
    return 'cr-dummy-lit' as const;
  }

  override render() {
    return html`
      <div id="foo">Hello Foo</div>
      <div id="bar">Hello Bar</div>
    `;
  }
}

customElements.define(CrDummyLitElement.is, CrDummyLitElement);

class CrDummyPropertiesWithNotifyElement extends CrLitElement {
  static get is() {
    return 'cr-dummy-properties-with-notify' as const;
  }

  static override get properties() {
    return {
      prop1: {
        type: Boolean,
        notify: true,
      },

      prop2: {
        type: Boolean,
        notify: false,
      },

      prop3: {type: Boolean},

      propFour: {
        type: Boolean,
        notify: true,
      },

      prop5: {
        type: Boolean,
        notify: true,
      },
    };
  }

  prop1: boolean = false;
  prop2: boolean = false;
  prop3: boolean = false;
  propFour: boolean = false;
  prop5: boolean|undefined = false;
}

customElements.define(
    CrDummyPropertiesWithNotifyElement.is, CrDummyPropertiesWithNotifyElement);

declare global {
  interface HTMLElementTagNameMap {
    [CrDummyLitElement.is]: CrDummyLitElement;
    [CrDummyPropertiesWithNotifyElement.is]: CrDummyPropertiesWithNotifyElement;
  }
}

suite('CrLitElement', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('ForcedRendering_ConnectedCallback', function() {
    const element = document.createElement('cr-dummy-lit');
    assertNull(element.shadowRoot);
    document.body.appendChild(element);

    // Test that the order of lifecycle callbacks is as expected.
    assertDeepEquals(
        ['connectedCallback', 'performUpdate'], element.lifecycleCallbacks);

    // Purposefully *not* calling `await element.updateComplete` here, to ensure
    // that initial rendering is synchronous when subclassing CrLitElement.

    assertNotEquals(null, element.shadowRoot);
  });

  // Called by cr-polymer-wrapper's connectedCallback() below. Exposed as a hook
  // to allow testing different cases.
  let polymerWrapperCallback: (e: CrDummyLitElement) => void = (_e) => {};

  // Defines two elements, cr-dom-if-polymer and cr-polymer-wrapper which are
  // used in a couple test cases.
  function defineForcedRenderingTestElements() {
    if (customElements.get('cr-polymer-wrapper')) {
      // Don't re-register the same elements, since this would lead to a runtime
      // error.
      return;
    }

    class CrPolymerWrapperElement extends PolymerElement {
      static get is() {
        return 'cr-polymer-wrapper';
      }

      static get template() {
        return polymerHtml`<cr-dummy-lit></cr-dummy-lit>`;
      }

      override connectedCallback() {
        super.connectedCallback();

        const litChild = this.shadowRoot!.querySelector('cr-dummy-lit');
        assertTrue(!!litChild);

        // Ensure that the problem is happening.
        assertNull(litChild.shadowRoot);
        assertDeepEquals([], litChild.lifecycleCallbacks);

        // Trigger the callback that is supposed to cause a forced-rendering.
        polymerWrapperCallback(litChild);

        // Check that the forced-rendering indeed happened.
        assertTrue(!!litChild.shadowRoot);

        // Check that 'performUpdate' was called, even though
        // 'connectedCallback' has not fired yet.
        assertDeepEquals(['performUpdate'], litChild.lifecycleCallbacks);

        litChild.updateComplete.then(() => {
          // Check that 'connectedCallback' and 'performUpdate' are called
          // exactly once some time later by Lit itself.
          assertDeepEquals(
              ['performUpdate', 'connectedCallback', 'performUpdate'],
              litChild.lifecycleCallbacks);
          this.dispatchEvent(new CustomEvent(
              'test-finished', {bubbles: true, composed: true}));
        });
      }
    }

    customElements.define(CrPolymerWrapperElement.is, CrPolymerWrapperElement);

    class CrDomIfPolymerElement extends PolymerElement {
      static get is() {
        return 'cr-dom-if-polymer';
      }

      static get template() {
        return polymerHtml`
          <template is="dom-if" if="[[show]]">
            <cr-polymer-wrapper></cr-polymer-wrapper>
          </template>
        `;
      }

      static get properties() {
        return {
          show: Boolean,
        };
      }

      show: boolean = false;

      override connectedCallback() {
        super.connectedCallback();
        this.show = true;
      }
    }

    customElements.define(CrDomIfPolymerElement.is, CrDomIfPolymerElement);
  }

  // Test an odd case where a CrLitElement is connected to the DOM but its
  // connectedCallback() method has not fired yet, which happens when the
  // following pattern is encountered:
  // dom-if > parent PolymerElement -> child CrLitElement
  // See CrLitElement definition for more details. Ensure that `shadowRoot` is
  // force-rendered as part of accessing the $ dictionary.
  test('ForcedRendering_BeforeConnectedCallback_DollarSign', function() {
    defineForcedRenderingTestElements();
    polymerWrapperCallback = (litChild: CrDummyLitElement) => {
      // Access the $ dictionary, to test that it causes the
      // `shadowRoot` to be force-rendered.
      assertTrue(!!litChild.$.foo);
    };

    const whenDone = eventToPromise('test-finished', document.body);
    document.body.innerHTML = getTrustedHTML`
      <cr-dom-if-polymer></cr-dom-if-polymer>
    `;
    return whenDone;
  });

  // Test an odd case where a CrLitElement is connected to the DOM but its
  // connectedCallback() method has not fired yet, which happens when the
  // following pattern is encountered:
  // dom-if > parent PolymerElement -> child CrLitElement
  // See CrLitElement definition for more details. Ensure that `shadowRoot` is
  // force-rendered as part of calling focus().
  test('ForcedRendering_BeforeConnectedCallback_Focus', function() {
    defineForcedRenderingTestElements();
    polymerWrapperCallback = (litChild: CrDummyLitElement) => {
      // Call focus() to test that whether causes the `shadowRoot` to be
      // force-rendered.
      litChild.focus();
    };

    const whenDone = eventToPromise('test-finished', document.body);
    document.body.innerHTML = getTrustedHTML`
      <cr-dom-if-polymer></cr-dom-if-polymer>
    `;
    return whenDone;
  });

  test('DollarSign_ErrorWhenNotConnectedOnce', function() {
    const element = document.createElement('cr-dummy-lit');
    assertDeepEquals([], element.lifecycleCallbacks);

    assertThrows(function() {
      element.$.foo;
      assertNotReached('Previous statement should have thrown an exception');
    }, 'CrLitElement CR-DUMMY-LIT $ dictionary accessed before element is connected at least once.');

    assertDeepEquals([], element.lifecycleCallbacks);
  });

  test('DollarSign_WorksAfterDisconnected', function() {
    const element = document.createElement('cr-dummy-lit');
    assertDeepEquals([], element.lifecycleCallbacks);
    document.body.appendChild(element);

    assertDeepEquals(
        ['connectedCallback', 'performUpdate'], element.lifecycleCallbacks);

    element.remove();
    assertDeepEquals(
        ['connectedCallback', 'performUpdate', 'disconnectedCallback'],
        element.lifecycleCallbacks);
    assertFalse(element.isConnected);
    assertNotEquals(null, element.$.foo);
  });

  test('DollarSign_WorksWhenConnected', function() {
    const element = document.createElement('cr-dummy-lit');
    document.body.appendChild(element);

    assertDeepEquals(
        ['connectedCallback', 'performUpdate'], element.lifecycleCallbacks);

    // Purposefully *not* calling `await element.updateComplete` here, to ensure
    // that initial rendering is synchronous when subclassing CrLitElement.

    assertTrue(!!element.$.foo);
    assertEquals('Hello Foo', element.$.foo.textContent);
    assertTrue(!!element.$.bar);
    assertEquals('Hello Bar', element.$.bar.textContent);

    // Test again lifecycle callbacks to ensure that performUpdate() has not
    // been called a second time as part of accessing the $ dictionary.
    assertDeepEquals(
        ['connectedCallback', 'performUpdate'], element.lifecycleCallbacks);
  });

  // Test that properties are initialized correctly from attributes.
  test('PropertiesAttributesNameMapping', function() {
    class CrDummyAttributesLitElement extends CrLitElement {
      static get is() {
        return 'cr-dummy-attributes-lit';
      }

      static override get properties() {
        return {
          fooBarBoolean: {type: Boolean},
          fooBarString: {type: String},

          fooBarStringCustom: {
            attribute: 'foobarstringcustom',
            type: String,
          },
        };
      }

      fooBarBoolean: boolean = false;
      fooBarString: string = 'hello';
      fooBarStringCustom: string = 'hola';
    }

    customElements.define(
        CrDummyAttributesLitElement.is, CrDummyAttributesLitElement);

    document.body.innerHTML = getTrustedHTML`
      <cr-dummy-attributes-lit foo-bar-boolean foo-bar-string="world"
          foobarstringcustom="custom">
      </cr-dummy-attributes-lit>
    `;

    const element = document.body.querySelector<CrDummyAttributesLitElement>(
        'cr-dummy-attributes-lit');
    assertTrue(!!element);
    assertTrue(element.fooBarBoolean);
    assertEquals('world', element.fooBarString);
    assertEquals('custom', element.fooBarStringCustom);
  });

  test('PropertiesWithNotify', async function() {
    const element = document.createElement('cr-dummy-properties-with-notify');

    function unexpectedEventListener(e: Event) {
      assertNotReached(`Unexpected event caught: ${e.type}`);
    }

    // Ensure that properties without 'notify: true' don't trigger events.
    element.addEventListener('prop2-changed', unexpectedEventListener);
    element.addEventListener('prop3-changed', unexpectedEventListener);

    // Ensure that properties with 'notify: true' that
    //   1) have a non-undefined initial value AND
    //   2) are changed back to undefined before the element is connected
    // also don't trigger updates.
    element.addEventListener('prop5-changed', unexpectedEventListener);
    element.prop5 = undefined;

    // Ensure that properties with 'notify: true' trigger events.

    // Case1: An event should be fired after the element is initialized to
    // propagate the initial value of the reactive property.
    const whenFired1 = Promise.all([
      eventToPromise('prop1-changed', element),
      eventToPromise('prop-four-changed', element),
    ]) as Promise<Array<CustomEvent<{value: boolean}>>>;

    document.body.appendChild(element);

    const events = await whenFired1;
    for (const event of events) {
      assertTrue(event.bubbles);
      assertTrue(event.composed);
      assertDeepEquals({value: false}, event.detail);
    }

    // Case2: An event should be fired whenever the property changes.
    let whenFired2 = eventToPromise('prop1-changed', element);
    element.prop1 = true;
    let event = await whenFired2;
    assertTrue(event.bubbles);
    assertTrue(event.composed);
    assertDeepEquals({value: true}, event.detail);

    whenFired2 = eventToPromise('prop-four-changed', element);
    element.propFour = true;
    event = await whenFired2;
    assertTrue(event.bubbles);
    assertTrue(event.composed);
    assertDeepEquals({value: true}, event.detail);
  });

  // Test that a Lit child with 'notify: true' properties works with a Polymer
  // parent that uses 2-way bindings for that property.
  test('PropertiesWithNotifyTwoWayBinding', async function() {
    class CrPolymerWrapperElement extends PolymerElement {
      static get is() {
        return 'cr-polymer-wrapper-with-two-way-binding';
      }

      static get template() {
        return polymerHtml`
            <cr-dummy-properties-with-notify prop1="{{myProp}}">
            </cr-dummy-properties-with-notify>`;
      }

      static get properties() {
        return {
          myProp: Boolean,
        };
      }

      myProp: boolean = false;
    }

    customElements.define(CrPolymerWrapperElement.is, CrPolymerWrapperElement);

    const parent =
        document.createElement('cr-polymer-wrapper-with-two-way-binding') as
        CrPolymerWrapperElement;
    document.body.appendChild(parent);

    const child =
        parent.shadowRoot!.querySelector('cr-dummy-properties-with-notify');
    assertTrue(!!child);
    assertFalse(child.prop1);
    assertFalse(parent.myProp);

    // Case1: Changes in child update parent's property.
    let whenFired = eventToPromise('prop1-changed', child);
    child.prop1 = true;
    await whenFired;
    assertTrue(parent.myProp);

    // Case2: Changes in parent update child's property.
    whenFired = eventToPromise('prop1-changed', child);
    parent.myProp = false;
    await whenFired;
    assertFalse(child.prop1);
  });

  test('Fire', async function() {
    const element = document.createElement('cr-dummy-lit');
    document.body.appendChild(element);

    const dummyEventName = 'dummy-event';
    const dummyPayload = 'hello dummy';

    const whenFired = eventToPromise(dummyEventName, element);
    element.fire(dummyEventName, dummyPayload);

    const event = await whenFired;
    assertTrue(event.bubbles);
    assertTrue(event.composed);
    assertEquals(dummyPayload, event.detail);
  });
});
