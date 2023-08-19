// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import './strings.m.js';
import './experiment.js';

import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {$, getDeepActiveElement, getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {FlagsExperimentElement} from './experiment.js';
import {ExperimentalFeaturesData, Feature, FlagsBrowserProxyImpl} from './flags_browser_proxy.js';

let lastChanged: HTMLElement|null = null;

// <if expr="not is_ios">
let lastFocused: HTMLElement|null = null;

const restartButton = getRequiredElement('experiment-restart-button');
// </if>

const experimentalFeaturesResolver: PromiseResolver<void> =
    new PromiseResolver();

// Exported on |window| since this is needed by tests.
Object.assign(
    window,
    {experimentalFeaturesReadyForTest: experimentalFeaturesResolver.promise});

interface Tab {
  tabEl: HTMLElement;
  panelEl: HTMLElement;
}

const tabs: Tab[] = [
  {
    tabEl: document.body.querySelector('#tab-available')!,
    panelEl: document.body.querySelector('#tab-content-available')!,
  },
  // <if expr="not is_ios">
  {
    tabEl: document.body.querySelector('#tab-unavailable')!,
    panelEl: document.body.querySelector('#tab-content-unavailable')!,
  },
  // </if>
];

/**
 * Toggles necessary attributes to display selected tab.
 */
function selectTab(selectedTabEl: HTMLElement) {
  for (const tab of tabs) {
    const isSelectedTab = tab.tabEl === selectedTabEl;
    tab.tabEl.classList.toggle('selected', isSelectedTab);
    tab.tabEl.setAttribute('aria-selected', String(isSelectedTab));
    tab.panelEl.classList.toggle('selected', isSelectedTab);
  }
}

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */

/**
 * Takes the |experimentalFeaturesData| input argument which represents data
 * about all the current feature entries and populates the page with
 * that data. It expects an object structure like the above.
 * @param experimentalFeaturesData Information about all experiments.
 */
function render(experimentalFeaturesData: ExperimentalFeaturesData) {
  const defaultFeatures: Feature[] = [];
  const nonDefaultFeatures: Feature[] = [];

  experimentalFeaturesData.supportedFeatures.forEach(
      f => (f.is_default ? defaultFeatures : nonDefaultFeatures).push(f));

  renderExperiments(
      nonDefaultFeatures, getRequiredElement('non-default-experiments'));

  renderExperiments(defaultFeatures, getRequiredElement('default-experiments'));

  // <if expr="not is_ios">
  renderExperiments(
      experimentalFeaturesData.unsupportedFeatures,
      getRequiredElement('unavailable-experiments'), true);
  // </if>

  showRestartToast(experimentalFeaturesData.needsRestart);

  // <if expr="not is_ios">
  restartButton.onclick = () =>
      FlagsBrowserProxyImpl.getInstance().restartBrowser();
  // </if>

  // Tab panel selection.
  for (const tab of tabs) {
    tab.tabEl.addEventListener('click', e => {
      e.preventDefault();
      selectTab(tab.tabEl);
    });
  }

  const resetAllButton = getRequiredElement('experiment-reset-all');
  resetAllButton.onclick = () => {
    resetAllFlags();
    lastChanged = resetAllButton;
  };
  registerFocusEvents(resetAllButton);

  // <if expr="is_chromeos">
  const crosUrlFlagsRedirectButton = $('os-link-href');
  if (crosUrlFlagsRedirectButton) {
    crosUrlFlagsRedirectButton.onclick =
        FlagsBrowserProxyImpl.getInstance().crosUrlFlagsRedirect;
  }
  // </if>

  highlightReferencedFlag();
}

/**
 * Add events to an element in order to keep track of the last focused element.
 * Focus restart button if a previous focus target has been set and tab key
 * pressed.
 */
function registerFocusEvents(el: HTMLElement) {
  el.addEventListener('keydown', function(e) {
    if (lastChanged && e.key === 'Tab' && !e.shiftKey) {
      e.preventDefault();
      // <if expr="not is_ios">
      lastFocused = lastChanged;
      restartButton.focus();
      // </if>
    }
  });
  el.addEventListener('blur', function() {
    lastChanged = null;
  });
}

/**
 * Highlight an element associated with the page's location's hash. We need to
 * fake fragment navigation with '.scrollIntoView()', since the fragment IDs
 * don't actually exist until after the template code runs; normal navigation
 * therefore doesn't work.
 */
function highlightReferencedFlag() {
  if (window.location.hash) {
    const experiment = document.body.querySelector(window.location.hash);
    if (!experiment) {
      return;
    }

    if (!experiment.classList.contains('referenced')) {
      // Unhighlight whatever's highlighted.
      const previous = document.body.querySelector('.referenced');
      if (previous) {
        previous.classList.remove('referenced');
      }
      // Highlight the referenced element.
      experiment.classList.add('referenced');

      // <if expr="not is_ios">
      // Switch to unavailable tab if the flag is in this section.
      if (getRequiredElement('tab-content-unavailable').contains(experiment)) {
        selectTab(getRequiredElement('tab-unavailable'));
      }
      // </if>
      experiment.scrollIntoView();
    }
  }
}

/**
 * Gets details and configuration about the available features. The
 * |returnExperimentalFeatures()| will be called with reply.
 */
function requestExperimentalFeaturesData() {
  FlagsBrowserProxyImpl.getInstance().requestExperimentalFeatures().then(
      returnExperimentalFeatures);
}

/**
 * Cause a text string to be announced by screen readers
 * @param text The text that should be announced.
 */
function announceStatus(text: string) {
  getRequiredElement('screen-reader-status-message').textContent = '';
  setTimeout(function() {
    getRequiredElement('screen-reader-status-message').textContent = text;
  }, 100);
}

/** Reset all flags to their default values and refresh the UI. */
function resetAllFlags() {
  FlagsBrowserProxyImpl.getInstance().resetAllFlags();
  FlagSearch.getInstance().clearSearch();
  announceStatus(loadTimeData.getString('reset-acknowledged'));
  showRestartToast(true);
  requestExperimentalFeaturesData();
}

function renderExperiments(
    features: Feature[], container: HTMLElement, unsupported = false) {
  const fragment = document.createDocumentFragment();
  for (const feature of features) {
    const experiment = document.createElement('flags-experiment');

    experiment.toggleAttribute('unsupported', unsupported);
    experiment.data = feature;
    experiment.id = feature.internal_name;

    const select = experiment.getSelect();
    if (select) {
      experiment.addEventListener('select-change', () => {
        showRestartToast(true);
        lastChanged = select;
        return false;
      });
      registerFocusEvents(select);
    }

    const textarea = experiment.getTextarea();
    if (textarea) {
      experiment.addEventListener('textarea-change', () => {
        showRestartToast(true);
        return false;
      });
    }
    const textbox = experiment.getTextbox();
    if (textbox) {
      experiment.addEventListener('input-change', () => {
        showRestartToast(true);
        return false;
      });
    }
    fragment.appendChild(experiment);
  }
  container.replaceChildren(fragment);
}

/**
 * Show the restart toast.
 * @param show Setting to toggle showing / hiding the toast.
 */
function showRestartToast(show: boolean) {
  getRequiredElement('needs-restart').classList.toggle('show', show);
  // There is no restart button on iOS.
  // <if expr="not is_ios">
  restartButton.setAttribute('tabindex', show ? '9' : '-1');
  // </if>
  if (show) {
    getRequiredElement('needs-restart').setAttribute('role', 'alert');
  }
}

/**
 * Called by the WebUI to re-populate the page with data representing the
 * current state of all experimental features.
 */
function returnExperimentalFeatures(
    experimentalFeaturesData: ExperimentalFeaturesData) {
  const bodyContainer = getRequiredElement('body-container');
  render(experimentalFeaturesData);

  if (experimentalFeaturesData.showBetaChannelPromotion) {
    getRequiredElement('channel-promo-beta').hidden = false;
  } else if (experimentalFeaturesData.showDevChannelPromotion) {
    getRequiredElement('channel-promo-dev').hidden = false;
  }

  getRequiredElement('promos').hidden =
      !experimentalFeaturesData.showBetaChannelPromotion &&
      !experimentalFeaturesData.showDevChannelPromotion;

  bodyContainer.style.visibility = 'visible';

  FlagSearch.getInstance().init();

  const ownerWarningDiv = $('owner-warning');
  if (ownerWarningDiv) {
    ownerWarningDiv.hidden = !experimentalFeaturesData.showOwnerWarning;
  }

  const systemFlagsLinkDiv = $('os-link-container');
  if (systemFlagsLinkDiv && !experimentalFeaturesData.showSystemFlagsLink) {
    systemFlagsLinkDiv.style.display = 'none';
  }

  experimentalFeaturesResolver.resolve();
}

// Delay in ms following a keypress, before a search is made.
const SEARCH_DEBOUNCE_TIME_MS: number = 150;

/**
 * Handles in page searching. Matches against the experiment flag name.
 */
class FlagSearch {
  private searchIntervalId_: number|null = null;

  private searchBox_: HTMLInputElement;
  private noMatchMsg_: NodeListOf<HTMLElement>;

  initialized: boolean = false;

  constructor() {
    this.searchBox_ = document.body.querySelector<HTMLInputElement>('#search')!;
    this.noMatchMsg_ = document.body.querySelectorAll('.tab-content .no-match');
  }

  /**
   * Initialises the in page search. Adding searchbox listeners and
   * collates the text elements used for string matching.
   */
  init() {
    if (!this.initialized) {
      this.searchBox_.addEventListener('input', this.debounceSearch.bind(this));

      document.body.querySelector('.clear-search')!.addEventListener(
          'click', this.clearSearch.bind(this));

      window.addEventListener('keyup', e => {
        // Check for an active textarea inside a <flags-experiment>.
        if (getDeepActiveElement()!.nodeName === 'TEXTAREA') {
          return;
        }
        switch (e.key) {
          case '/':
            this.searchBox_.focus();
            break;
          case 'Escape':
          case 'Enter':
            this.searchBox_.blur();
            break;
        }
      });
      this.searchBox_.focus();
      this.initialized = true;
    }
  }

  /**
   * Clears a search showing all experiments.
   */
  clearSearch() {
    this.searchBox_.value = '';
    this.doSearch();
  }

  /**
   * Goes through all experiment text and highlights the relevant matches.
   * Only the first instance of a match in each experiment text block is
   * highlighted. This prevents the sea of yellow that happens using the global
   * find in page search.
   * @param searchContent Object containing the experiment text elements to
   *     search against.
   * @return The number of matches found.
   */
  highlightAllMatches(
      experiments: NodeListOf<FlagsExperimentElement>,
      searchTerm: string): number {
    let matches = 0;
    for (const experiment of experiments) {
      const hasMatch = experiment.match(searchTerm);
      matches += hasMatch ? 1 : 0;
      experiment.classList.toggle('hidden', !hasMatch);
    }
    return matches;
  }

  /**
   * Performs a search against the experiment title, description, permalink.
   */
  doSearch() {
    const searchTerm = this.searchBox_.value.trim().toLowerCase();

    if (searchTerm || searchTerm === '') {
      document.body.classList.toggle('searching', Boolean(searchTerm));

      // Available experiments
      const availableExperiments =
          document.body.querySelectorAll<FlagsExperimentElement>(
              '#tab-content-available flags-experiment');
      this.noMatchMsg_[0]!.classList.toggle(
          'hidden',
          this.highlightAllMatches(availableExperiments, searchTerm) > 0);

      // <if expr="not is_ios">
      // Unavailable experiments, which are undefined on iOS.
      const unavailableExperiments =
          document.body.querySelectorAll<FlagsExperimentElement>(
              '#tab-content-unavailable flags-experiment');
      this.noMatchMsg_[1]!.classList.toggle(
          'hidden',
          this.highlightAllMatches(unavailableExperiments, searchTerm) > 0);
      // </if>
      this.announceSearchResults();
    }

    this.searchIntervalId_ = null;
  }

  announceSearchResults() {
    const searchTerm = this.searchBox_.value.trim().toLowerCase();
    if (!searchTerm) {
      return;
    }

    const selectedTab =
        tabs.find(tab => tab.panelEl.classList.contains('selected'))!;
    const selectedTabId = selectedTab.panelEl.id;
    const queryString = `#${selectedTabId} flags-experiment:not(.hidden)`;
    const total = document.body.querySelectorAll(queryString).length;
    if (total) {
      announceStatus(
          total === 1 ?
              loadTimeData.getStringF('searchResultsSingular', searchTerm) :
              loadTimeData.getStringF(
                  'searchResultsPlural', total, searchTerm));
    }
  }

  /**
   * Debounces the search to improve performance and prevent too many searches
   * from being initiated.
   */
  debounceSearch() {
    if (this.searchIntervalId_) {
      clearTimeout(this.searchIntervalId_);
    }
    this.searchIntervalId_ =
        setTimeout(this.doSearch.bind(this), SEARCH_DEBOUNCE_TIME_MS);
  }

  /** Get the singleton instance of FlagSearch. */
  static getInstance(): FlagSearch {
    return instance || (instance = new FlagSearch());
  }
}

let instance: FlagSearch|null = null;

// <if expr="not is_ios">
/**
 * Allows the restart button to jump back to the previously focused experiment
 * in the list instead of going to the top of the page.
 */
function setupRestartButton() {
  restartButton.addEventListener('keydown', function(e) {
    if (e.shiftKey && e.key === 'Tab' && lastFocused) {
      e.preventDefault();
      lastFocused.focus();
    }
  });
  restartButton.addEventListener('blur', () => {
    lastFocused = null;
  });
}
// </if>

document.addEventListener('DOMContentLoaded', function() {
  // Get and display the data upon loading.
  requestExperimentalFeaturesData();
  // There is no restart button on iOS.
  // <if expr="not is_ios">
  setupRestartButton();
  // </if>
  FocusOutlineManager.forDocument(document);
});

// Update the highlighted flag when the hash changes.
window.addEventListener('hashchange', highlightReferencedFlag);
