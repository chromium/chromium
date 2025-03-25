// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of browser tests for the Search and Assistant settings card element.
 */

import type {IronCollapseElement, OsSettingsRoutes, SearchAndAssistantSettingsCardElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

suite('<search-and-assistant-settings-card>', () => {
  const defaultRoute = routes.SYSTEM_PREFERENCES;

  const ALLOWED_ENTERPRISE_POLICIES = [
    {desc: 'allowed with model improvement', value: 0},
    {desc: 'allowed without model improvement', value: 1},
    {desc: 'an invalid value', value: 3},
  ] as const satisfies ReadonlyArray<{desc: string, value: number}>;

  let searchAndAssistantSettingsCard: SearchAndAssistantSettingsCardElement;

  function createSearchAndAssistantCard() {
    searchAndAssistantSettingsCard =
        document.createElement('search-and-assistant-settings-card');
    document.body.appendChild(searchAndAssistantSettingsCard);
    flush();
  }

  setup(() => {
    loadTimeData.overrideValues({
      isAssistantAllowed: false,
      isQuickAnswersSupported: false,
    });
  });

  teardown(() => {
    searchAndAssistantSettingsCard.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('when Quick Answers is supported', () => {
    setup(() => {
      loadTimeData.overrideValues({isQuickAnswersSupported: true});
    });

    test('Search subpage row should be visible', () => {
      createSearchAndAssistantCard();
      const searchRow =
          searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              '#searchRow');
      assertTrue(isVisible(searchRow));
    });

    test('Search engine row should not be stamped', () => {
      createSearchAndAssistantCard();
      const searchEngineRow =
          searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              'settings-search-engine');
      assertNull(searchEngineRow);
    });
  });

  suite('Magic Boost setting toggle', () => {
    test('should appear if is isMagicBoostFeatureEnabled flag is true', () => {
      loadTimeData.overrideValues({
        isMagicBoostFeatureEnabled: true,
      });
      createSearchAndAssistantCard();
      assertTrue(
          isVisible(searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              '#magicBoostToggle')));
    });

    test(
        'should be hidden if isMagicBoostFeatureEnabled flag is false.', () => {
          loadTimeData.overrideValues({
            isMagicBoostFeatureEnabled: false,
          });
          createSearchAndAssistantCard();
          assertNull(searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              '#magicBoostToggle'));
        });

    test('reflects pref value and collapse the sub items', () => {
      loadTimeData.overrideValues({
        isMagicBoostFeatureEnabled: true,
      });
      createSearchAndAssistantCard();
      const fakePrefs = {
        settings: {
          magic_boost_enabled: {
            value: true,
          },
        },
      };
      searchAndAssistantSettingsCard.prefs = fakePrefs;
      flush();

      const magicBoostToggle =
          searchAndAssistantSettingsCard.shadowRoot!
              .querySelector<SettingsToggleButtonElement>('#magicBoostToggle');

      assertTrue(!!magicBoostToggle);
      assertTrue(magicBoostToggle.checked);
      assertTrue(searchAndAssistantSettingsCard.get(
          'prefs.settings.magic_boost_enabled.value'));

      const magicBoostCollapse =
          searchAndAssistantSettingsCard.shadowRoot!
              .querySelector<IronCollapseElement>('#magicBoostCollapse');
      assertTrue(!!magicBoostCollapse);
      assertTrue(magicBoostCollapse.opened);

      // Click the toggle change the value of the pref, and fold the collapse.
      magicBoostToggle.click();
      assertFalse(magicBoostToggle.checked);
      assertFalse(searchAndAssistantSettingsCard.get(
          'prefs.settings.magic_boost_enabled.value'));
      assertFalse(magicBoostCollapse.opened);
    });

    test('Magic Boost toggle is deep-linkable', async () => {
      loadTimeData.overrideValues({
        isMagicBoostFeatureEnabled: true,
      });
      createSearchAndAssistantCard();

      const setting = settingMojom.Setting.kMagicBoostOnOff;
      const params = new URLSearchParams();
      params.append('settingId', setting.toString());
      Router.getInstance().navigateTo(defaultRoute, params);

      const deepLinkElement =
          searchAndAssistantSettingsCard.shadowRoot!.querySelector<HTMLElement>(
              '#magicBoostToggle');
      assertTrue(!!deepLinkElement);

      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement,
          searchAndAssistantSettingsCard.shadowRoot!.activeElement,
          `Element should be focused for settingId=${setting}.'`);
    });

    test('sub items are deep-linkable', async () => {
      loadTimeData.overrideValues({
        isMagicBoostFeatureEnabled: true,
        isLobsterSettingsToggleVisible: true,
      });
      createSearchAndAssistantCard();
      const fakePrefs = {
        settings: {
          magic_boost_enabled: {
            value: true,
          },
        },
      };
      searchAndAssistantSettingsCard.prefs = fakePrefs;
      flush();

      const subItems = new Map<settingMojom.Setting, string>([
        [settingMojom.Setting.kMahiOnOff, '#helpMeReadToggle'],
        [settingMojom.Setting.kShowOrca, '#helpMeWriteToggle'],
        [settingMojom.Setting.kLobsterOnOff, '#lobsterToggle'],

      ]);

      for (const [setting, element] of subItems) {
        const params = new URLSearchParams();
        params.append('settingId', setting.toString());
        Router.getInstance().navigateTo(defaultRoute, params);

        const deepLinkElement = searchAndAssistantSettingsCard.shadowRoot!
                                    .querySelector<HTMLElement>(element);
        assertTrue(!!deepLinkElement);

        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement,
            searchAndAssistantSettingsCard.shadowRoot!.activeElement,
            `Element should be focused for settingId=${setting}.'`);
      }
    });

    suite('Hmr enterprise policy', () => {
      setup(() => {
        loadTimeData.overrideValues({
          isMagicBoostFeatureEnabled: true,
        });
      });

      for (const {desc, value} of ALLOWED_ENTERPRISE_POLICIES) {
        suite(`is ${desc}`, () => {
          let hmrToggle: SettingsToggleButtonElement;

          setup(() => {
            createSearchAndAssistantCard();
            searchAndAssistantSettingsCard.prefs = {
              settings: {
                magic_boost_enabled: {
                  value: true,
                  type: chrome.settingsPrivate.PrefType.BOOLEAN,
                },
                mahi_enabled: {
                  value: true,
                  type: chrome.settingsPrivate.PrefType.BOOLEAN,
                },
                managed: {
                  help_me_read: {
                    value,
                    type: chrome.settingsPrivate.PrefType.NUMBER,
                  },
                },
              },
            };
            flush();

            const nullableHmrToggle =
                searchAndAssistantSettingsCard.shadowRoot!
                    .querySelector<SettingsToggleButtonElement>(
                        '#helpMeReadToggle');
            assertTrue(nullableHmrToggle !== null);
            hmrToggle = nullableHmrToggle;
          });

          test('Hmr toggle should appear', () => {
            assertTrue(isVisible(hmrToggle));
          });

          test('Hmr enterprise toggle should not appear', () => {
            const hmrEnterpriseToggle =
                searchAndAssistantSettingsCard.shadowRoot!
                    .querySelector<SettingsToggleButtonElement>(
                        '#helpMeReadEnterpriseToggle');
            assertFalse(isVisible(hmrEnterpriseToggle));
          });

          test('Hmr toggle reflects pref value', () => {
            assertTrue(isVisible(hmrToggle));
            assertTrue(hmrToggle.checked);
            assertTrue(searchAndAssistantSettingsCard.get(
                'prefs.settings.mahi_enabled.value'));

            hmrToggle.click();
            assertFalse(hmrToggle.checked);
            assertFalse(searchAndAssistantSettingsCard.get(
                'prefs.settings.mahi_enabled.value'));
          });

          test(
              'then changes to disallowed, ' +
                  'Hmr enterprise toggle is deep-linkable',
              async () => {
                searchAndAssistantSettingsCard.set(
                    'prefs.settings.managed.help_me_read.value', 2);
                flush();

                const hmrEnterpriseToggle =
                    searchAndAssistantSettingsCard.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#helpMeReadEnterpriseToggle');
                assertTrue(hmrEnterpriseToggle !== null);

                const setting = settingMojom.Setting.kMahiOnOff;
                const params = new URLSearchParams();
                params.append('settingId', setting.toString());
                Router.getInstance().navigateTo(defaultRoute, params);

                await waitAfterNextRender(hmrEnterpriseToggle);
                assertEquals(
                    hmrEnterpriseToggle,
                    searchAndAssistantSettingsCard.shadowRoot!.activeElement,
                    `Element should be focused for settingId=${setting}.'`);
              });
        });
      }

      suite('is disallowed', () => {
        let hmrEnterpriseToggle: SettingsToggleButtonElement;

        setup(() => {
          createSearchAndAssistantCard();
          searchAndAssistantSettingsCard.prefs = {
            settings: {
              magic_boost_enabled: {
                value: true,
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
              },
              mahi_enabled: {
                value: true,
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
              },
              managed: {
                help_me_read: {
                  value: 2,
                  type: chrome.settingsPrivate.PrefType.NUMBER,
                },
              },
            },
          };
          flush();

          const nullableHmrEnterpriseToggle =
              searchAndAssistantSettingsCard.shadowRoot!
                  .querySelector<SettingsToggleButtonElement>(
                      '#helpMeReadEnterpriseToggle');
          assertTrue(nullableHmrEnterpriseToggle !== null);
          hmrEnterpriseToggle = nullableHmrEnterpriseToggle;
        });

        test('Hmr enterprise toggle should appear', () => {
          assertTrue(isVisible(hmrEnterpriseToggle));
        });

        test('Hmr toggle should not appear', () => {
          const hmrToggle = searchAndAssistantSettingsCard.shadowRoot!
                                .querySelector<SettingsToggleButtonElement>(
                                    '#helpMeReadToggle');
          assertFalse(isVisible(hmrToggle));
        });

        test('Hmr enterprise toggle appears unchecked', () => {
          assertTrue(isVisible(hmrEnterpriseToggle));
          assertFalse(hmrEnterpriseToggle.checked);
        });

        test('Hmr enterprise toggle does not respond to clicks', () => {
          assertTrue(isVisible(hmrEnterpriseToggle));
          hmrEnterpriseToggle.click();

          assertFalse(hmrEnterpriseToggle.checked);
          assertTrue(searchAndAssistantSettingsCard.get(
              'prefs.settings.mahi_enabled.value'));
          assertEquals(
              2,
              searchAndAssistantSettingsCard.get(
                  'prefs.settings.managed.help_me_read.value'));
        });

        test('Hmr enterprise toggle is deep-linkable', async () => {
          const setting = settingMojom.Setting.kMahiOnOff;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(defaultRoute, params);

          await waitAfterNextRender(hmrEnterpriseToggle);
          assertEquals(
              hmrEnterpriseToggle,
              searchAndAssistantSettingsCard.shadowRoot!.activeElement,
              `Element should be focused for settingId=${setting}.'`);
        });

        for (const {desc, value} of ALLOWED_ENTERPRISE_POLICIES) {
          test(
              `then changes to ${desc}, Hmr toggle is deep-linkable`,
              async () => {
                searchAndAssistantSettingsCard.set(
                    'prefs.settings.managed.help_me_read.value', value);
                flush();

                const hmrToggle =
                    searchAndAssistantSettingsCard.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#helpMeReadToggle');
                assertTrue(hmrToggle !== null);

                const setting = settingMojom.Setting.kMahiOnOff;
                const params = new URLSearchParams();
                params.append('settingId', setting.toString());
                Router.getInstance().navigateTo(defaultRoute, params);

                await waitAfterNextRender(hmrToggle);
                assertEquals(
                    hmrToggle,
                    searchAndAssistantSettingsCard.shadowRoot!.activeElement,
                    `Element should be focused for settingId=${setting}.'`);
              });
        }
      });
    });

    suite('Help me write enterprise policy', () => {
      setup(() => {
        loadTimeData.overrideValues({
          isMagicBoostFeatureEnabled: true,
        });
      });

      for (const {desc, value} of ALLOWED_ENTERPRISE_POLICIES) {
        suite(`is ${desc}`, () => {
          let hmwToggle: SettingsToggleButtonElement;

          setup(() => {
            createSearchAndAssistantCard();
            searchAndAssistantSettingsCard.prefs = {
              assistive_input: {
                orca_enabled: {
                  value: true,
                  type: chrome.settingsPrivate.PrefType.BOOLEAN,
                },
              },
              settings: {
                magic_boost_enabled: {
                  value: true,
                  type: chrome.settingsPrivate.PrefType.BOOLEAN,
                },
                managed: {
                  help_me_write: {
                    value,
                    type: chrome.settingsPrivate.PrefType.NUMBER,
                  },
                },
              },

            };
            flush();

            const nullableHmwToggle =
                searchAndAssistantSettingsCard.shadowRoot!
                    .querySelector<SettingsToggleButtonElement>(
                        '#helpMeWriteToggle');
            assertTrue(nullableHmwToggle !== null);
            hmwToggle = nullableHmwToggle;
          });

          test('Hmw toggle should appear', () => {
            assertTrue(isVisible(hmwToggle));
          });

          test('Hmw enterprise toggle should not appear', () => {
            const hmwEnterpriseToggle =
                searchAndAssistantSettingsCard.shadowRoot!
                    .querySelector<SettingsToggleButtonElement>(
                        '#helpMeWriteEnterpriseToggle');
            assertFalse(isVisible(hmwEnterpriseToggle));
          });

          test('Hmw toggle reflects pref value', () => {
            assertTrue(isVisible(hmwToggle));
            assertTrue(hmwToggle.checked);
            assertTrue(searchAndAssistantSettingsCard.get(
                'prefs.assistive_input.orca_enabled.value'));

            hmwToggle.click();
            assertFalse(hmwToggle.checked);
            assertFalse(searchAndAssistantSettingsCard.get(
                'prefs.assistive_input.orca_enabled.value'));
          });

          test(
              'then changes to disallowed, ' +
                  'Hmw enterprise toggle is deep-linkable',
              async () => {
                searchAndAssistantSettingsCard.set(
                    'prefs.settings.managed.help_me_write.value', 2);
                flush();

                const hmwEnterpriseToggle =
                    searchAndAssistantSettingsCard.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#helpMeWriteEnterpriseToggle');
                assertTrue(hmwEnterpriseToggle !== null);

                const setting = settingMojom.Setting.kShowOrca;
                const params = new URLSearchParams();
                params.append('settingId', setting.toString());
                Router.getInstance().navigateTo(defaultRoute, params);

                await waitAfterNextRender(hmwEnterpriseToggle);
                assertEquals(
                    hmwEnterpriseToggle,
                    searchAndAssistantSettingsCard.shadowRoot!.activeElement,
                    `Element should be focused for settingId=${setting}.'`);
              });
        });
      }

      suite('is disallowed', () => {
        let hmwEnterpriseToggle: SettingsToggleButtonElement;

        setup(() => {
          createSearchAndAssistantCard();
          searchAndAssistantSettingsCard.prefs = {
            settings: {
              magic_boost_enabled: {
                value: true,
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
              },
              managed: {
                help_me_write: {
                  value: 2,
                  type: chrome.settingsPrivate.PrefType.NUMBER,
                },
              },
            },
            assistive_input: {
              orca_enabled: {
                value: true,
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
              },
            },
          };
          flush();

          const nullableHmwEnterpriseToggle =
              searchAndAssistantSettingsCard.shadowRoot!
                  .querySelector<SettingsToggleButtonElement>(
                      '#helpMeWriteEnterpriseToggle');
          assertTrue(nullableHmwEnterpriseToggle !== null);
          hmwEnterpriseToggle = nullableHmwEnterpriseToggle;
        });

        test('Hmw enterprise toggle should appear', () => {
          assertTrue(isVisible(hmwEnterpriseToggle));
        });

        test('Hmw toggle should not appear', () => {
          const hmwToggle = searchAndAssistantSettingsCard.shadowRoot!
                                .querySelector<SettingsToggleButtonElement>(
                                    '#helpMeWriteToggle');
          assertFalse(isVisible(hmwToggle));
        });

        test('Hmw enterprise toggle appears unchecked', () => {
          assertTrue(isVisible(hmwEnterpriseToggle));
          assertFalse(hmwEnterpriseToggle.checked);
        });

        test('Hmw enterprise toggle does not respond to clicks', () => {
          assertTrue(isVisible(hmwEnterpriseToggle));
          hmwEnterpriseToggle.click();

          assertFalse(hmwEnterpriseToggle.checked);
          assertTrue(searchAndAssistantSettingsCard.get(
              'prefs.assistive_input.orca_enabled.value'));
          assertEquals(
              2,
              searchAndAssistantSettingsCard.get(
                  'prefs.settings.managed.help_me_write.value'));
        });

        test('Hmw enterprise toggle is deep-linkable', async () => {
          const setting = settingMojom.Setting.kShowOrca;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(defaultRoute, params);

          await waitAfterNextRender(hmwEnterpriseToggle);
          assertEquals(
              hmwEnterpriseToggle,
              searchAndAssistantSettingsCard.shadowRoot!.activeElement,
              `Element should be focused for settingId=${setting}.'`);
        });

        for (const {desc, value} of ALLOWED_ENTERPRISE_POLICIES) {
          test(
              `then changes to ${desc}, Hmw toggle is deep-linkable`,
              async () => {
                searchAndAssistantSettingsCard.set(
                    'prefs.settings.managed.help_me_write.value', value);
                flush();

                const hmwToggle =
                    searchAndAssistantSettingsCard.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#helpMeWriteToggle');
                assertTrue(hmwToggle !== null);

                const setting = settingMojom.Setting.kShowOrca;
                const params = new URLSearchParams();
                params.append('settingId', setting.toString());
                Router.getInstance().navigateTo(defaultRoute, params);

                await waitAfterNextRender(hmwToggle);
                assertEquals(
                    hmwToggle,
                    searchAndAssistantSettingsCard.shadowRoot!.activeElement,
                    `Element should be focused for settingId=${setting}.'`);
              });
        }
      });
    });

    suite('Lobster setting toggle', () => {
      suite('should be hidden', () => {
        [{
          isMagicBoostFeatureEnabled: false,
          isLobsterSettingsToggleVisible: false,
        },
         {
           isMagicBoostFeatureEnabled: false,
           isLobsterSettingsToggleVisible: true,
         },
         {
           isMagicBoostFeatureEnabled: true,
           isLobsterSettingsToggleVisible: false,
         }].forEach(({
                      isMagicBoostFeatureEnabled,
                      isLobsterSettingsToggleVisible,
                    }) => {
          test(
              `when isMagicBoostFeatureEnabled is ${
                  isMagicBoostFeatureEnabled
                      .toString()} and isLobsterSettingsToggleVisible is ${
                  isLobsterSettingsToggleVisible.toString()}`,
              () => {
                loadTimeData.overrideValues({
                  isMagicBoostFeatureEnabled,
                  isLobsterSettingsToggleVisible,
                });
                createSearchAndAssistantCard();
                searchAndAssistantSettingsCard.prefs = {
                  settings: {
                    magic_boost_enabled: {
                      value: true,
                    },
                  },
                };
                flush();
                assertFalse(isVisible(
                    searchAndAssistantSettingsCard.shadowRoot!.querySelector(
                        '#lobsterToggle')));
                assertFalse(isVisible(
                    searchAndAssistantSettingsCard.shadowRoot!.querySelector(
                        '#lobsterEnterpriseToggle')));
              });
        });
      });

      suite(
          'should be visible when isMagicBoostFeatureEnabled and' +
              ' isLobsterSettingsToggleVisible are both true, and ',
          () => {
            setup(() => {
              loadTimeData.overrideValues({
                isMagicBoostFeatureEnabled: true,
                isLobsterSettingsToggleVisible: true,
              });
            });
            suite('when Lobster enterprise policy enables the feature', () => {
              for (const {desc, value} of ALLOWED_ENTERPRISE_POLICIES) {
                suite(`is ${desc}`, () => {
                  let lobsterToggle: SettingsToggleButtonElement;

                  setup(() => {
                    createSearchAndAssistantCard();
                    searchAndAssistantSettingsCard.prefs = {
                      settings: {
                        magic_boost_enabled: {
                          value: true,
                          type: chrome.settingsPrivate.PrefType.BOOLEAN,
                        },
                        lobster: {
                          enterprise_settings: {
                            value: value,
                            type: chrome.settingsPrivate.PrefType.NUMBER,
                          },
                        },
                        lobster_enabled: {
                          value: true,
                          type: chrome.settingsPrivate.PrefType.BOOLEAN,
                        },
                      },
                    };
                    flush();

                    const nullableLobsterToggle =
                        searchAndAssistantSettingsCard.shadowRoot!
                            .querySelector<SettingsToggleButtonElement>(
                                '#lobsterToggle');
                    assertTrue(nullableLobsterToggle !== null);
                    lobsterToggle = nullableLobsterToggle;
                  });

                  test('Lobster toggle should appear', () => {
                    assertTrue(isVisible(lobsterToggle));
                  });

                  test('Lobster enterprise toggle should not appear', () => {
                    const lobsterEnterpriseToggle =
                        searchAndAssistantSettingsCard.shadowRoot!
                            .querySelector<SettingsToggleButtonElement>(
                                '#lobsterEnterpriseToggle');
                    assertFalse(isVisible(lobsterEnterpriseToggle));
                  });

                  test('Lobster toggle reflects pref value', () => {
                    assertTrue(isVisible(lobsterToggle));
                    assertTrue(lobsterToggle.checked);
                    assertTrue(searchAndAssistantSettingsCard.get(
                        'prefs.settings.lobster_enabled.value'));

                    lobsterToggle.click();
                    assertFalse(lobsterToggle.checked);
                    assertFalse(searchAndAssistantSettingsCard.get(
                        'prefs.settings.lobster_enabled.value'));
                  });

                  test(
                      'then changes to disallowed, Lobster enterprise toggle' +
                          ' is deep-linkable',
                      async () => {
                        searchAndAssistantSettingsCard.set(
                            'prefs.settings.lobster.enterprise_settings.value',
                            2);
                        flush();

                        const lobsterEnterpriseToggle =
                            searchAndAssistantSettingsCard.shadowRoot!
                                .querySelector<SettingsToggleButtonElement>(
                                    '#lobsterEnterpriseToggle');
                        assertTrue(lobsterEnterpriseToggle !== null);

                        const setting = settingMojom.Setting.kLobsterOnOff;
                        const params = new URLSearchParams();
                        params.append('settingId', setting.toString());
                        Router.getInstance().navigateTo(defaultRoute, params);

                        await waitAfterNextRender(lobsterEnterpriseToggle);
                        assertEquals(
                            lobsterEnterpriseToggle,
                            searchAndAssistantSettingsCard.shadowRoot!
                                .activeElement,
                            `Element should be focused for settingId=${
                                setting}.'`);
                      });
                });
              }
            });
            suite('when Lobster enterprise policy disables the feature, ', () => {
              let lobsterEnterpriseToggle: SettingsToggleButtonElement;
              setup(() => {
                createSearchAndAssistantCard();
                searchAndAssistantSettingsCard.prefs = {
                  settings: {
                    lobster: {
                      enterprise_settings: {
                        value: 2,
                        type: chrome.settingsPrivate.PrefType.NUMBER,
                      },
                    },
                    lobster_enabled: {
                      value: true,
                      type: chrome.settingsPrivate.PrefType.BOOLEAN,
                    },
                    magic_boost_enabled: {
                      value: true,
                      type: chrome.settingsPrivate.PrefType.BOOLEAN,
                    },
                  },
                };
                flush();
                const nullableLobsterEnterpriseToggle =
                    searchAndAssistantSettingsCard.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#lobsterEnterpriseToggle');
                assertTrue(nullableLobsterEnterpriseToggle !== null);
                lobsterEnterpriseToggle = nullableLobsterEnterpriseToggle;
              });

              test('Lobster enterprise toggle should appear', () => {
                assertTrue(isVisible(lobsterEnterpriseToggle));
              });

              test('Lobster toggle should not appear', () => {
                const lobsterToggle =
                    searchAndAssistantSettingsCard.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#lobsterToggle');
                assertFalse(isVisible(lobsterToggle));
              });

              test('Lobster enterprise toggle appears unchecked', () => {
                assertTrue(isVisible(lobsterEnterpriseToggle));
                assertFalse(lobsterEnterpriseToggle.checked);
              });

              test(
                  'Lobster enterprise toggle does not respond to clicks',
                  () => {
                    assertTrue(isVisible(lobsterEnterpriseToggle));
                    lobsterEnterpriseToggle.click();

                    assertFalse(lobsterEnterpriseToggle.checked);
                    assertTrue(searchAndAssistantSettingsCard.get(
                        'prefs.settings.lobster_enabled.value'));
                    assertEquals(
                        2,
                        searchAndAssistantSettingsCard.get(
                            'prefs.settings.lobster.enterprise_settings.value'));
                  });

              test('Lobster enterprise toggle is deep-linkable', async () => {
                const setting = settingMojom.Setting.kLobsterOnOff;
                const params = new URLSearchParams();
                params.append('settingId', setting.toString());
                Router.getInstance().navigateTo(defaultRoute, params);

                await waitAfterNextRender(lobsterEnterpriseToggle);
                assertEquals(
                    lobsterEnterpriseToggle,
                    searchAndAssistantSettingsCard.shadowRoot!.activeElement,
                    `Element should be focused for settingId=${setting}.'`);
              });

              for (const {desc, value} of ALLOWED_ENTERPRISE_POLICIES) {
                test(
                    `then changes to ${desc}, Lobster toggle is deep-linkable`,
                    async () => {
                      searchAndAssistantSettingsCard.set(
                          'prefs.settings.lobster.enterprise_settings.value',
                          value);
                      flush();

                      const lobsterToggle =
                          searchAndAssistantSettingsCard.shadowRoot!
                              .querySelector<SettingsToggleButtonElement>(
                                  '#lobsterToggle');
                      assertTrue(lobsterToggle !== null);

                      const setting = settingMojom.Setting.kLobsterOnOff;
                      const params = new URLSearchParams();
                      params.append('settingId', setting.toString());
                      Router.getInstance().navigateTo(defaultRoute, params);

                      await waitAfterNextRender(lobsterToggle);
                      assertEquals(
                          lobsterToggle,
                          searchAndAssistantSettingsCard.shadowRoot!
                              .activeElement,
                          `Element should be focused for settingId=${
                              setting}.'`);
                    });
              }
            });
          });
    });
  });

  test(
      'when isScannerSettingsToggleVisible flag is false, ' +
          'Scanner toggles are hidden',
      () => {
        loadTimeData.overrideValues({
          isScannerSettingsToggleVisible: false,
        });
        createSearchAndAssistantCard();
        assertFalse(
            isVisible(searchAndAssistantSettingsCard.shadowRoot!.querySelector(
                '#scannerToggle')));
        assertFalse(
            isVisible(searchAndAssistantSettingsCard.shadowRoot!.querySelector(
                '#scannerEnterpriseToggle')));
      });

  suite('when isScannerSettingsToggleVisible flag is true', () => {
    setup(() => {
      loadTimeData.overrideValues({
        isScannerSettingsToggleVisible: true,
      });
    });

    for (const {desc, value} of ALLOWED_ENTERPRISE_POLICIES) {
      suite(`and enterprise policy is ${desc}`, () => {
        let scannerToggle: SettingsToggleButtonElement;

        setup(() => {
          createSearchAndAssistantCard();
          searchAndAssistantSettingsCard.prefs = {
            ash: {
              scanner: {
                enabled: {
                  value: true,
                  type: chrome.settingsPrivate.PrefType.BOOLEAN,
                },
                enterprise_policy_allowed: {
                  value,
                  type: chrome.settingsPrivate.PrefType.NUMBER,
                },
              },
            },
          };
          flush();

          const nullableScannerToggle =
              searchAndAssistantSettingsCard.shadowRoot!
                  .querySelector<SettingsToggleButtonElement>('#scannerToggle');
          assertTrue(nullableScannerToggle !== null);
          scannerToggle = nullableScannerToggle;
        });

        test('Scanner toggle should appear', () => {
          assertTrue(isVisible(scannerToggle));
        });

        test('Scanner enterprise toggle should not appear', () => {
          const scannerEnterpriseToggle =
              searchAndAssistantSettingsCard.shadowRoot!
                  .querySelector<SettingsToggleButtonElement>(
                      '#scannerEnterpriseToggle');
          assertTrue(!isVisible(scannerEnterpriseToggle));
        });

        test('Scanner toggle reflects pref value', () => {
          assertTrue(isVisible(scannerToggle));
          assertTrue(scannerToggle.checked);
          assertTrue(searchAndAssistantSettingsCard.get(
              'prefs.ash.scanner.enabled.value'));

          scannerToggle.click();
          assertFalse(scannerToggle.checked);
          assertFalse(searchAndAssistantSettingsCard.get(
              'prefs.ash.scanner.enabled.value'));
        });

        test('Scanner toggle is deep-linkable', async () => {
          const setting = settingMojom.Setting.kScannerOnOff;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(defaultRoute, params);

          await waitAfterNextRender(scannerToggle);
          assertEquals(
              scannerToggle,
              searchAndAssistantSettingsCard.shadowRoot!.activeElement,
              `Element should be focused for settingId=${setting}.'`);
        });

        test(
            'then changes to disallowed, ' +
                'Scanner enterprise toggle is deep-linkable',
            async () => {
              searchAndAssistantSettingsCard.set(
                  'prefs.ash.scanner.enterprise_policy_allowed.value', 2);
              flush();

              const scannerEnterpriseToggle =
                  searchAndAssistantSettingsCard.shadowRoot!
                      .querySelector<SettingsToggleButtonElement>(
                          '#scannerEnterpriseToggle');
              assertTrue(scannerEnterpriseToggle !== null);

              const setting = settingMojom.Setting.kScannerOnOff;
              const params = new URLSearchParams();
              params.append('settingId', setting.toString());
              Router.getInstance().navigateTo(defaultRoute, params);

              await waitAfterNextRender(scannerEnterpriseToggle);
              assertEquals(
                  scannerEnterpriseToggle,
                  searchAndAssistantSettingsCard.shadowRoot!.activeElement,
                  `Element should be focused for settingId=${setting}.'`);
            });
      });
    }

    suite('and enterprise policy is disallowed', () => {
      let scannerEnterpriseToggle: SettingsToggleButtonElement;

      setup(() => {
        createSearchAndAssistantCard();
        searchAndAssistantSettingsCard.prefs = {
          ash: {
            scanner: {
              enabled: {
                value: true,
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
              },
              enterprise_policy_allowed: {
                value: 2,
                type: chrome.settingsPrivate.PrefType.NUMBER,
              },
            },
          },
        };
        flush();

        const nullableScannerEnterpriseToggle =
            searchAndAssistantSettingsCard.shadowRoot!
                .querySelector<SettingsToggleButtonElement>(
                    '#scannerEnterpriseToggle');
        assertTrue(nullableScannerEnterpriseToggle !== null);
        scannerEnterpriseToggle = nullableScannerEnterpriseToggle;
      });

      test('Scanner enterprise toggle should appear', () => {
        assertTrue(isVisible(scannerEnterpriseToggle));
      });

      test('Scanner toggle should not appear', () => {
        const scannerToggle =
            searchAndAssistantSettingsCard.shadowRoot!
                .querySelector<SettingsToggleButtonElement>('#scannerToggle');
        assertTrue(!isVisible(scannerToggle));
      });

      test('Scanner enterprise toggle appears unchecked', () => {
        assertTrue(isVisible(scannerEnterpriseToggle));
        assertFalse(scannerEnterpriseToggle.checked);
      });

      test('Scanner enterprise toggle does not respond to clicks', () => {
        assertTrue(isVisible(scannerEnterpriseToggle));
        scannerEnterpriseToggle.click();

        assertFalse(scannerEnterpriseToggle.checked);
        assertTrue(searchAndAssistantSettingsCard.get(
            'prefs.ash.scanner.enabled.value'));
        assertEquals(
            searchAndAssistantSettingsCard.get(
                'prefs.ash.scanner.enterprise_policy_allowed.value'),
            2);
      });

      test('Scanner enterprise toggle is deep-linkable', async () => {
        const setting = settingMojom.Setting.kScannerOnOff;
        const params = new URLSearchParams();
        params.append('settingId', setting.toString());
        Router.getInstance().navigateTo(defaultRoute, params);

        await waitAfterNextRender(scannerEnterpriseToggle);
        assertEquals(
            scannerEnterpriseToggle,
            searchAndAssistantSettingsCard.shadowRoot!.activeElement,
            `Element should be focused for settingId=${setting}.'`);
      });

      for (const {desc, value} of ALLOWED_ENTERPRISE_POLICIES) {
        test(
            `then changes to ${desc}, Scanner toggle is deep-linkable`,
            async () => {
              searchAndAssistantSettingsCard.set(
                  'prefs.ash.scanner.enterprise_policy_allowed.value', value);
              flush();

              const scannerToggle =
                  searchAndAssistantSettingsCard.shadowRoot!
                      .querySelector<SettingsToggleButtonElement>(
                          '#scannerToggle');
              assertTrue(scannerToggle !== null);

              const setting = settingMojom.Setting.kScannerOnOff;
              const params = new URLSearchParams();
              params.append('settingId', setting.toString());
              Router.getInstance().navigateTo(defaultRoute, params);

              await waitAfterNextRender(scannerToggle);
              assertEquals(
                  scannerToggle,
                  searchAndAssistantSettingsCard.shadowRoot!.activeElement,
                  `Element should be focused for settingId=${setting}.'`);
            });
      }
    });
  });

  suite('when Quick Answers is not supported', () => {
    test('Search engine row should be visible', () => {
      createSearchAndAssistantCard();
      const searchEngineRow =
          searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              'settings-search-engine');
      assertTrue(isVisible(searchEngineRow));
    });

    test('Search subpage row should not be stamped', () => {
      createSearchAndAssistantCard();
      const searchRow =
          searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              '#searchRow');
      assertNull(searchRow);
    });

    test('Search engine select is deep linkable', async () => {
      createSearchAndAssistantCard();

      const params = new URLSearchParams();
      params.append('settingId', '600');
      Router.getInstance().navigateTo(defaultRoute, params);

      const settingsSearchEngine =
          searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              'settings-search-engine');
      assertTrue(!!settingsSearchEngine);

      const browserSearchSettingsLink =
          settingsSearchEngine.shadowRoot!.querySelector(
              '#browserSearchSettingsLink');
      assertTrue(!!browserSearchSettingsLink);

      const deepLinkElement =
          browserSearchSettingsLink.shadowRoot!.querySelector('cr-icon-button');
      assertTrue(!!deepLinkElement);

      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Preferred search dropdown should be focused for settingId=600.');
    });
  });

  suite('when Assistant settings are available', () => {
    setup(() => {
      loadTimeData.overrideValues({isAssistantAllowed: true});
    });

    test('Assistant row should be visible', () => {
      createSearchAndAssistantCard();
      const assistantRow =
          searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              '#assistantRow');
      assertTrue(isVisible(assistantRow));
    });
  });

  suite('when Assistant settings are not available', () => {
    test('Assistant row should not be stamped', () => {
      createSearchAndAssistantCard();
      const assistantRow =
          searchAndAssistantSettingsCard.shadowRoot!.querySelector(
              '#assistantRow');
      assertNull(assistantRow);
    });
  });

  const subpageTriggerData: SubpageTriggerData[] = [
    {
      triggerSelector: '#searchRow',
      routeName: 'SEARCH_SUBPAGE',
    },
    {
      triggerSelector: '#assistantRow',
      routeName: 'GOOGLE_ASSISTANT',
    },
  ];
  subpageTriggerData.forEach(({triggerSelector, routeName}) => {
    test(
        `Row for ${routeName} is focused when returning from subpage`,
        async () => {
          loadTimeData.overrideValues({
            isAssistantAllowed: true,       // Show google assistant row
            isQuickAnswersSupported: true,  // Show quick answers row
          });
          createSearchAndAssistantCard();

          Router.getInstance().navigateTo(defaultRoute);

          const subpageTrigger =
              searchAndAssistantSettingsCard.shadowRoot!
                  .querySelector<HTMLElement>(triggerSelector);
          assertTrue(!!subpageTrigger);

          // Sub-page trigger navigates to subpage for route
          subpageTrigger.click();
          assertEquals(routes[routeName], Router.getInstance().currentRoute);

          // Navigate back
          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(searchAndAssistantSettingsCard);

          assertEquals(
              subpageTrigger,
              searchAndAssistantSettingsCard.shadowRoot!.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });

  test('Content recommendations toggle is visible', () => {
    createSearchAndAssistantCard();
    const contentRecommendationsToggle =
        searchAndAssistantSettingsCard.shadowRoot!.querySelector(
            '#contentRecommendationsToggle');
    assertTrue(isVisible(contentRecommendationsToggle));
  });

  test('Content recommendations toggle reflects pref value', () => {
    createSearchAndAssistantCard();
    const fakePrefs = {
      settings: {
        suggested_content_enabled: {
          value: true,
        },
      },
    };
    searchAndAssistantSettingsCard.prefs = fakePrefs;
    flush();

    const contentRecommendationsToggle =
        searchAndAssistantSettingsCard.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#contentRecommendationsToggle');
    assertTrue(!!contentRecommendationsToggle);

    assertTrue(contentRecommendationsToggle.checked);
    assertTrue(searchAndAssistantSettingsCard.get(
        'prefs.settings.suggested_content_enabled.value'));

    contentRecommendationsToggle.click();
    assertFalse(contentRecommendationsToggle.checked);
    assertFalse(searchAndAssistantSettingsCard.get(
        'prefs.settings.suggested_content_enabled.value'));
  });
});
