// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://metrics-internals/app.js';

import {MetricsInternalsBrowserProxyImpl} from 'chrome://metrics-internals/browser_proxy.js';
import type {FieldTrialState, HashNameMap, KeyValue, MetricsInternalsBrowserProxy, Trial} from 'chrome://metrics-internals/browser_proxy.js';
import type {FieldTrialsAppElement} from 'chrome://metrics-internals/field_trials.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

function wait(): Promise<void> {
  return new Promise(resolve => {
    window.setTimeout(() => {
      resolve();
    }, 1);
  });
}

class FakeBrowser extends TestBrowserProxy implements
    MetricsInternalsBrowserProxy {
  trialState: FieldTrialState = {
    trials: [],
    restartRequired: false,
  };
  lookupTrialOrGroupNameResult: HashNameMap = {};

  constructor() {
    super([
      'getUmaLogData',
      'fetchVariationsSummary',
      'fetchUmaSummary',
      'isUsingMetricsServiceObserver',
      'setTrialEnrollState',
      'fetchTrialState',
      'lookupTrialOrGroupName',
      'restart',
    ]);
  }

  // Note: Each browser proxy method uses `wait()` so that the returned
  // promise isn't immediately resolved. This mirrors the actual browser
  // proxy more accurately.

  async getUmaLogData(includeLogProtoData: boolean): Promise<string> {
    this.methodCalled('getUmaLogData', includeLogProtoData);
    await wait();
    return '';
  }

  async fetchVariationsSummary(): Promise<KeyValue[]> {
    this.methodCalled('fetchVariationsSummary');
    await wait();
    return [];
  }

  async fetchUmaSummary(): Promise<KeyValue[]> {
    this.methodCalled('fetchUmaSummary');
    await wait();
    return [];
  }

  async isUsingMetricsServiceObserver(): Promise<boolean> {
    this.methodCalled('isUsingMetricsServiceObserver');
    await wait();
    return false;
  }

  async setTrialEnrollState(
      trialHash: string, groupHash: string,
      forceEnable: boolean): Promise<boolean> {
    this.methodCalled('setTrialEnrollState', trialHash, groupHash, forceEnable);
    await wait();
    return false;
  }

  async fetchTrialState(): Promise<FieldTrialState> {
    this.methodCalled('fetchTrialState');
    await wait();
    return this.trialState;
  }

  async lookupTrialOrGroupName(name: string): Promise<HashNameMap> {
    this.methodCalled('lookupTrialOrGroupName', name);
    await wait();
    return this.lookupTrialOrGroupNameResult;
  }

  async restart(): Promise<void> {
    this.methodCalled('restart');
    await wait();
  }
}

function makeTrial(name: string): Trial {
  return {
    name,
    hash: btoa(name),
    groups: [
      {
        name: 'On',
        hash: '1',
        enabled: false,
        forceEnabled: false,
      },
      {
        name: 'Off',
        hash: '2',
        enabled: false,
        forceEnabled: false,
      },
    ],
  };
}

interface DisplayedTrial {
  title: string;
  groups: DisplayedGroup[];
}

interface DisplayedGroup {
  title: string;
  enrolled: boolean;
  overridden: boolean;
}


suite('FieldTrialsTest', function() {
  let app: FieldTrialsAppElement;
  let fakeBrowser: FakeBrowser;

  /** Returns the list of field trials currently displayed. */
  const getDisplayedTrials = async(): Promise<DisplayedTrial[]> => {
    await waitForUpdate();
    const displayedTrials: DisplayedTrial[] = [];
    for (const trialDiv of app.shadowRoot!.querySelectorAll<HTMLElement>(
             '#field-trial-list .trial-row')) {
      if (!trialDiv.checkVisibility()) {
        continue;
      }
      const groups: DisplayedGroup[] =
          Array.from(trialDiv.querySelectorAll<HTMLElement>('.experiment-row'))
              .map(experimentRow => ({
                     title: experimentRow.querySelector('.experiment-name')!
                                .textContent!.trim(),
                     enrolled: experimentRow.dataset['enrolled'] === '1',
                     overridden: experimentRow
                                     .querySelector<HTMLInputElement>(
                                         '.override input')!.checked,
                   }));
      if (groups.length) {
        displayedTrials.push({
          title: trialDiv.querySelector('.trial-header')!.textContent!.trim(),
          groups,
        });
      }
    }
    return displayedTrials;
  };

  const filterInputElement = (): HTMLInputElement => {
    const filterInput = app.shadowRoot!.getElementById('filter');
    assertNotEquals(filterInput, null);
    return filterInput as HTMLInputElement;
  };

  async function waitForUpdate() {
    while (app.dirty) {
      await new Promise<void>(resolve => {
        app.onUpdateForTesting = () => {
          resolve();
        };
      });
    }
  }

  async function makeApp() {
    app = document.createElement('field-trials-app');
    document.body.replaceChildren(app);
    await waitForUpdate();
  }

  setup(() => {
    MetricsInternalsBrowserProxyImpl.setInstance(
        fakeBrowser = new FakeBrowser());
    localStorage.clear();
  });

  teardown(() => {
    MetricsInternalsBrowserProxyImpl.setInstance(
        new MetricsInternalsBrowserProxyImpl());
  });

  test('page loads with real proxy', async function() {
    MetricsInternalsBrowserProxyImpl.setInstance(
        new MetricsInternalsBrowserProxyImpl());
    await makeApp();
    assertTrue(app != null);
    assertTrue(app.shadowRoot != null);
    const blurbContainer = app.shadowRoot.querySelector('.blurb-container');
    assertTrue(blurbContainer != null);
    const blurbText = (blurbContainer as HTMLElement).innerText;
    assertTrue(
        blurbText.includes('WARNING: EXPERIMENTAL FEATURES AHEAD!'),
        'Found text: ' + blurbText);
  });

  test('page loads', async function() {
    await makeApp();
    assertTrue(app != null);
    assertTrue(app.shadowRoot != null);
    const blurbContainer = app.shadowRoot.querySelector('.blurb-container');
    assertTrue(blurbContainer != null);
    const blurbText = (blurbContainer as HTMLElement).innerText;
    assertTrue(
        blurbText.includes('WARNING: EXPERIMENTAL FEATURES AHEAD!'),
        'Found text: ' + blurbText);
  });

  test('populateState shows trials', async () => {
    const red = makeTrial('Red');
    red.groups[0]!.enabled = true;
    const blue = makeTrial('Blue');
    fakeBrowser.trialState = {
      trials: [blue, red],
      restartRequired: false,
    };
    await makeApp();
    const displayedTrials = await getDisplayedTrials();
    assertDeepEquals(
        [
          {
            title: 'Blue (#Qmx1ZQ==)',
            groups: [
              {
                title: 'On (#1)',
                enrolled: false,
                overridden: false,
              },
              {
                title: 'Off (#2)',
                enrolled: false,
                overridden: false,
              },
            ],
          },
          {
            title: 'Red (#UmVk)',
            groups: [
              {
                title: 'On (#1)',
                enrolled: true,
                overridden: false,
              },
              {
                title: 'Off (#2)',
                enrolled: false,
                overridden: false,
              },
            ],
          },
        ],
        displayedTrials, 'got ' + JSON.stringify(displayedTrials));
  });

  test('force enroll tells browser', async function() {
    fakeBrowser.trialState = {
      trials: [makeTrial('Red')],
      restartRequired: false,
    };
    await makeApp();
    // Click the first enroll checkbox.
    app.shadowRoot!.querySelector<HTMLInputElement>('.override input')!.click();
    assertDeepEquals(
        ['UmVk', '1', true],
        await fakeBrowser.whenCalled('setTrialEnrollState'));
  });

  test('force enroll allows one per trial', async function() {
    fakeBrowser.trialState = {
      trials: [makeTrial('Red')],
      restartRequired: false,
    };
    await makeApp();

    // Click the first and then the second enroll checkboxes.
    const checkboxes = Array.from(
        app.shadowRoot!.querySelectorAll<HTMLInputElement>('.override input'));
    assertEquals(checkboxes.length, 2);
    checkboxes[0]!.click();
    assertDeepEquals(
        ['UmVk', '1', true],
        await fakeBrowser.whenCalled('setTrialEnrollState'),
        'after clicking first box');

    fakeBrowser.reset();
    checkboxes[1]!.click();
    assertDeepEquals(
        ['UmVk', '2', true],
        await fakeBrowser.whenCalled('setTrialEnrollState'),
        'after clicking second box');

    assertEquals(checkboxes[0]!.checked, false);
    assertEquals(checkboxes[1]!.checked, true);
  });

  test('filter by trial name matches', async function() {
    const trial = makeTrial('Red');
    trial.hash = 'somehash';
    fakeBrowser.trialState = {
      trials: [trial],
      restartRequired: false,
    };
    await makeApp();

    filterInputElement().value = 'Red';
    filterInputElement().dispatchEvent(new Event('input'));

    assertEquals((await getDisplayedTrials()).length, 1);
  });

  test('filter by trial hash matches', async function() {
    const trial = makeTrial('Red');
    trial.hash = 'somehash';
    fakeBrowser.trialState = {
      trials: [trial],
      restartRequired: false,
    };
    await makeApp();

    filterInputElement().value = 'somehash';
    filterInputElement().dispatchEvent(new Event('input'));

    assertEquals((await getDisplayedTrials()).length, 1);
  });

  test('filter no match', async function() {
    const trial = makeTrial('Red');
    trial.hash = 'somehash';
    fakeBrowser.trialState = {
      trials: [trial],
      restartRequired: false,
    };
    await makeApp();

    filterInputElement().value = 'Blue';
    filterInputElement().dispatchEvent(new Event('input'));

    assertEquals((await getDisplayedTrials()).length, 0);
  });

  test('populate without names displays hash only', async function() {
    const trial = makeTrial('Red');
    trial.name = undefined;
    trial.hash = 'somehash';
    fakeBrowser.trialState = {
      trials: [trial],
      restartRequired: false,
    };
    await makeApp();

    const displayedTrials = await getDisplayedTrials();
    assertEquals('#somehash', displayedTrials[0]!.title);
  });

  test('filter by name can unmask hash', async function() {
    const trial = makeTrial('Red');
    trial.name = undefined;
    trial.hash = 'somehash';
    fakeBrowser.trialState = {
      trials: [trial],
      restartRequired: false,
    };
    await makeApp();

    fakeBrowser.reset();
    fakeBrowser.lookupTrialOrGroupNameResult = {'somehash': 'Red'};

    filterInputElement().value = 'Red';
    filterInputElement().dispatchEvent(new Event('input'));

    assertEquals('Red', await fakeBrowser.whenCalled('lookupTrialOrGroupName'));

    const displayedTrials = await getDisplayedTrials();
    assertEquals(
        'Red (#somehash)', displayedTrials[0]!.title,
        JSON.stringify(displayedTrials));
  });
});
