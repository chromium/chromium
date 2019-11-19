// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('metrics reporting', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsPrivacyPageElement} */
  let page;

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-personalization-options');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('changes to whether metrics reporting is enabled/managed', function() {
    let toggled;
    return testBrowserProxy.whenCalled('getMetricsReporting')
        .then(function() {
          return test_util.flushTasks();
        })
        .then(function() {
          const control = page.$.metricsReportingControl;
          assertEquals(
              testBrowserProxy.metricsReporting.enabled, control.checked);
          assertEquals(
              testBrowserProxy.metricsReporting.managed,
              !!control.pref.controlledBy);

          const changedMetrics = {
            enabled: !testBrowserProxy.metricsReporting.enabled,
            managed: !testBrowserProxy.metricsReporting.managed,
          };
          cr.webUIListenerCallback('metrics-reporting-change', changedMetrics);
          Polymer.dom.flush();

          assertEquals(changedMetrics.enabled, control.checked);
          assertEquals(changedMetrics.managed, !!control.pref.controlledBy);

          toggled = !changedMetrics.enabled;
          control.checked = toggled;
          control.notifyChangedByUserInteraction();

          return testBrowserProxy.whenCalled('setMetricsReportingEnabled');
        })
        .then(function(enabled) {
          assertEquals(toggled, enabled);
        });
  });

  test('metrics reporting restart button', function() {
    return testBrowserProxy.whenCalled('getMetricsReporting').then(function() {
      Polymer.dom.flush();

      // Restart button should be hidden by default (in any state).
      assertFalse(!!page.$$('#restart'));

      // Simulate toggling via policy.
      cr.webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: true,
      });

      // No restart button should show because the value is managed.
      assertFalse(!!page.$$('#restart'));

      cr.webUIListenerCallback('metrics-reporting-change', {
        enabled: true,
        managed: true,
      });
      Polymer.dom.flush();

      // Changes in policy should not show the restart button because the value
      // is still managed.
      assertFalse(!!page.$$('#restart'));

      // Remove the policy and toggle the value.
      cr.webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: false,
      });
      Polymer.dom.flush();

      // Now the restart button should be showing.
      assertTrue(!!page.$$('#restart'));

      // Receiving the same values should have no effect.
      cr.webUIListenerCallback('metrics-reporting-change', {
        enabled: false,
        managed: false,
      });
      Polymer.dom.flush();
      assertTrue(!!page.$$('#restart'));
    });
  });
});
