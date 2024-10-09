// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import './strings.m.js';
import './experiment.js';

import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {ExperimentElement as FlagsExperimentElement} from './experiment.js';
import type {ExperimentalFeaturesData, Feature} from './flags_browser_proxy.js';
import {FlagsBrowserProxyImpl} from './flags_browser_proxy.js';


/**
 * Goes through all experiment text and highlights the relevant matches.
 * Only the first instance of a match in each experiment text block is
 * highlighted. This prevents the sea of yellow that happens using the
 * global find in page search.
 * @param experiments The list of elements to search on and highlight.
 * @param searchTerm The query to search for.
 * @return The number of matches found.
 */
async function highlightAllMatches(
    experiments: NodeListOf<FlagsExperimentElement>,
    searchTerm: string): Promise<number> {
  let matches = 0;
  // Not using for..of with async/await to spawn all searching in parallel.
  await Promise.all(Array.from(experiments).map(async (experiment) => {
    const hasMatch = await experiment.match(searchTerm);
    matches += hasMatch ? 1 : 0;
    experiment.hidden = !hasMatch;
  }));
  return matches;
}

/**
 * Handles in page searching. Matches against the experiment flag name.
 */
class FlagSearch {
  private flagsAppElement: FlagsAppElement;
  private searchIntervalId: number|null = null;
  // Delay in ms following a keypress, before a search is made.
  private searchDebounceDelayMs: number = 150;

  constructor(el: FlagsAppElement) {
    this.flagsAppElement = el;
  }

  /**
   * Performs a search against the experiment title, description, platforms and
   * permalink text.
   */
  async doSearch() {
    await this.flagsAppElement.search();
    this.searchIntervalId = null;
  }

  /**
   * Debounces the search to improve performance and prevent too many searches
   * from being initiated.
   */
  debounceSearch() {
    if (this.searchIntervalId) {
      clearTimeout(this.searchIntervalId);
    }
    this.searchIntervalId =
        setTimeout(this.doSearch.bind(this), this.searchDebounceDelayMs);
  }

  setSearchDebounceDelayMsForTesting(delay: number) {
    this.searchDebounceDelayMs = delay;
  }
}

export interface FlagsAppElement {
  $: {
    search: HTMLInputElement,
  };
}

export class FlagsAppElement extends CrLitElement {
  static get is() {
    return 'flags-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
      defaultFeatures: {type: Array},
      nonDefaultFeatures: {type: Array},

      searching: {
        type: Boolean,
        reflect: true,
      },

      tabNames_: {type: Array},
      selectedTabIndex_: {type: Number},
    };
  }

  protected tabNames_: string[] = [
    loadTimeData.getString('available'),
    // <if expr="not is_ios">
    loadTimeData.getString('unavailable'),
    // </if>
  ];
  protected selectedTabIndex_: number = 0;

  protected data: ExperimentalFeaturesData = {
    supportedFeatures: [],
    // <if expr="not is_ios">
    unsupportedFeatures: [],
    // </if>
    needsRestart: false,
    showBetaChannelPromotion: false,
    showDevChannelPromotion: false,
    // <if expr="chromeos_ash">
    showOwnerWarning: false,
    // </if>
    // <if expr="chromeos_lacros or chromeos_ash">
    showSystemFlagsLink: false,
    // </if>
  };

  protected defaultFeatures: Feature[] = [];
  protected nonDefaultFeatures: Feature[] = [];
  protected searching: boolean = false;

  private announceStatusDelayMs: number = 100;
  private featuresResolver: PromiseResolver<void> = new PromiseResolver();
  private flagSearch: FlagSearch|null = null;
  private lastChanged: HTMLElement|null = null;
  // <if expr="not is_ios">
  private lastFocused: HTMLElement|null = null;

  // Whether the current URL is chrome://flags/deprecated. Only updated on
  // initial load.
  private isFlagsDeprecatedUrl_: boolean = false;
  // </if>

  private eventTracker_: EventTracker|null = null;

  getRequiredElement<K extends keyof HTMLElementTagNameMap>(query: K):
      HTMLElementTagNameMap[K];
  getRequiredElement<E extends HTMLElement = HTMLElement>(query: string): E;
  getRequiredElement(query: string) {
    const el = this.shadowRoot!.querySelector(query);
    assert(el);
    assert(el instanceof HTMLElement);
    return el;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('data')) {
      const defaultFeatures: Feature[] = [];
      const nonDefaultFeatures: Feature[] = [];

      this.data.supportedFeatures.forEach(
          f => (f.is_default ? defaultFeatures : nonDefaultFeatures).push(f));

      this.defaultFeatures = defaultFeatures;
      this.nonDefaultFeatures = nonDefaultFeatures;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.flagSearch = new FlagSearch(this);

    this.$.search.focus();
  }

  override async updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    this.showRestartToast(this.data.needsRestart);
    if (this.defaultFeatures.length === 0 &&
        // <if expr="not is_ios">
        this.data.unsupportedFeatures.length === 0 &&
        // </if>
        this.nonDefaultFeatures.length === 0) {
      // Return early if this update corresponds to the initial dummy data, to
      // avoid triggering `featuresResolver` prematurely in tests.
      return;
    }

    await this.highlightReferencedFlag();
    this.featuresResolver.resolve();
  }

  // <if expr="not is_ios">
  private getRestartButton(): HTMLButtonElement {
    return this.getRequiredElement<HTMLButtonElement>(
        '#experiment-restart-button');
  }
  // </if>

  override connectedCallback() {
    super.connectedCallback();

    // <if expr="not is_ios">
    const pathname = new URL(window.location.href).pathname;
    this.isFlagsDeprecatedUrl_ =
        ['/deprecated', '/deprecated/test_loader.html'].includes(pathname);
    // </if>

    // Get and display the data upon loading.
    this.requestExperimentalFeaturesData();

    FocusOutlineManager.forDocument(document);

    // <if expr="not is_ios">
    if (this.isFlagsDeprecatedUrl_) {
      // Update strings that are slightly different when on
      // chrome://flags/deprecated
      document.title = loadTimeData.getString('deprecatedTitle');
      this.getRequiredElement('.section-header-title').textContent =
          loadTimeData.getString('deprecatedHeading');
      this.getRequiredElement('.blurb-warning').textContent = '';
      this.getRequiredElement('.blurb-warning + span').textContent =
          loadTimeData.getString('deprecatedPageWarningExplanation');
      this.$.search.placeholder =
          loadTimeData.getString('deprecatedSearchPlaceholder');
      for (const element of this.shadowRoot!.querySelectorAll('.no-match')) {
        element.textContent = loadTimeData.getString('deprecatedNoResults');
      }
    }
    // </if>

    this.eventTracker_ = new EventTracker();

    // Update the highlighted flag when the hash changes.
    this.eventTracker_.add(
        window, 'hashchange', () => this.highlightReferencedFlag());

    this.eventTracker_.add(window, 'keyup', (e: KeyboardEvent) => {
      // Check for an active textarea inside a <flags-experiment>.
      const activeElement = getDeepActiveElement();
      if (activeElement && activeElement.nodeName === 'TEXTAREA') {
        return;
      }
      switch (e.key) {
        case '/':
          this.$.search.focus();
          break;
        case 'Escape':
          this.$.search.blur();
          break;
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.eventTracker_);
    this.eventTracker_.removeAll();
    this.eventTracker_ = null;
  }

  setAnnounceStatusDelayMsForTesting(delay: number) {
    this.announceStatusDelayMs = delay;
  }

  setSearchDebounceDelayMsForTesting(delay: number) {
    assert(this.flagSearch);
    this.flagSearch.setSearchDebounceDelayMsForTesting(delay);
  }

  experimentalFeaturesReadyForTesting() {
    return this.featuresResolver.promise;
  }

  /**
   * Cause a text string to be announced by screen readers
   * @param text The text that should be announced.
   */
  announceStatus(text: string): Promise<void> {
    return new Promise((resolve) => {
      this.getRequiredElement('#screen-reader-status-message').textContent = '';
      setTimeout(() => {
        this.getRequiredElement('#screen-reader-status-message').textContent =
            text;
        resolve();
      }, this.announceStatusDelayMs);
    });
  }

  async search() {
    const searchTerm = this.$.search.value.trim().toLowerCase();

    this.searching = Boolean(searchTerm);

    // Available experiments
    const availableExperiments =
        this.shadowRoot!.querySelectorAll<FlagsExperimentElement>(
            '#tab-content-available flags-experiment');
    const availableExperimentsHits =
        await highlightAllMatches(availableExperiments, searchTerm);
    let noMatchMsg =
        this.getRequiredElement('#tab-content-available .no-match');
    noMatchMsg.toggleAttribute('hidden', availableExperimentsHits > 0);

    // <if expr="not is_ios">
    // Unavailable experiments, which are undefined on iOS.
    const unavailableExperiments =
        this.shadowRoot!.querySelectorAll<FlagsExperimentElement>(
            '#tab-content-unavailable flags-experiment');
    const unavailableExperimentsHits =
        await highlightAllMatches(unavailableExperiments, searchTerm);
    noMatchMsg = this.getRequiredElement('#tab-content-unavailable .no-match');
    noMatchMsg.toggleAttribute('hidden', unavailableExperimentsHits > 0);
    // </if>

    if (this.searching) {
      // <if expr="is_ios">
      await this.announceSearchResults(searchTerm, availableExperimentsHits);
      // </if>
      // <if expr="not is_ios">
      const hits = this.selectedTabIndex_ === 0 ? availableExperimentsHits :
                                                  unavailableExperimentsHits;
      await this.announceSearchResults(searchTerm, hits);
      // </if>
    }

    await this.updateComplete;
    this.dispatchEvent(new Event('search-finished-for-testing', {
      bubbles: true,
      composed: true,
    }));
  }

  private announceSearchResults(searchTerm: string, total: number):
      Promise<void> {
    if (total) {
      return this.announceStatus(
          total === 1 ?
              loadTimeData.getStringF('searchResultsSingular', searchTerm) :
              loadTimeData.getStringF(
                  'searchResultsPlural', total, searchTerm));
    }
    return Promise.resolve();
  }

  /*
   * Focus restart button if a previous focus target has been set and
   * tab key pressed.
   */
  protected onResetAllKeydown_(e: KeyboardEvent) {
    if (this.lastChanged && e.key === 'Tab' && !e.shiftKey) {
      e.preventDefault();
      // <if expr="not is_ios">
      this.lastFocused = this.lastChanged;
      this.getRestartButton().focus();
      // </if>
    }
  }

  protected onResetAllBlur_() {
    this.lastChanged = null;
  }

  /**
   * Highlight an element associated with the page's location's hash. We need to
   * fake fragment navigation with '.scrollIntoView()', since the fragment IDs
   * don't actually exist until after the template code runs; normal navigation
   * therefore doesn't work.
   */
  private async highlightReferencedFlag() {
    if (!window.location.hash) {
      return;
    }

    const experiment = this.shadowRoot!.querySelector(window.location.hash);
    if (!experiment || experiment.classList.contains('referenced')) {
      return;
    }

    // Unhighlight whatever's highlighted.
    const previous = this.shadowRoot!.querySelector('.referenced');
    if (previous) {
      previous.classList.remove('referenced');
    }
    // Highlight the referenced element.
    experiment.classList.add('referenced');

    // <if expr="not is_ios">
    // Switch to unavailable tab if the flag is in this section.
    if (this.getRequiredElement('#tab-content-unavailable')
            .contains(experiment)) {
      this.selectedTabIndex_ = 1;
      await this.updateComplete;
      await this.getRequiredElement('cr-tabs').updateComplete;
    }
    // </if>
    experiment.scrollIntoView();
  }

  /**
   * Gets details and configuration about the available features.
   */
  private async requestExperimentalFeaturesData() {
    // <if expr="not is_ios">
    const data = this.isFlagsDeprecatedUrl_ ?
        await FlagsBrowserProxyImpl.getInstance().requestDeprecatedFeatures() :
        await FlagsBrowserProxyImpl.getInstance().requestExperimentalFeatures();
    // </if>
    // <if expr="is_ios">
    const data =
        await FlagsBrowserProxyImpl.getInstance().requestExperimentalFeatures();
    // </if>

    this.data = data;
  }

  /**
   * Clears a search showing all experiments.
   */
  private clearSearch() {
    this.$.search.value = '';
    assert(this.flagSearch);
    this.flagSearch.doSearch();
    this.$.search.focus();
  }

  /** Reset all flags to their default values and refresh the UI. */
  protected async onResetAllClick_(e: Event) {
    this.lastChanged = e.target as HTMLElement;
    FlagsBrowserProxyImpl.getInstance().resetAllFlags();
    this.announceStatus(loadTimeData.getString('reset-acknowledged'));
    this.showRestartToast(true);

    await this.requestExperimentalFeaturesData();
    await this.updateComplete;

    this.clearSearch();
  }

  protected onSearchInput_() {
    assert(this.flagSearch);
    this.flagSearch.debounceSearch();
  }

  protected onClearSearchClick_() {
    this.clearSearch();
  }

  protected onSelectChange_(e: Event) {
    const select = e.composedPath()[0];
    assert(select instanceof HTMLSelectElement);
    this.showRestartToast(true);

    if (this.lastChanged === select) {
      return;
    }

    this.lastChanged = select;

    // Add listeners so that next 'Tab' keystroke focuses the restart button.
    const eventTracker = new EventTracker();
    eventTracker.add(select, 'keydown', (e: KeyboardEvent) => {
      if (e.key === 'Tab' && !e.shiftKey) {
        assert(this.lastChanged === select);
        e.preventDefault();
        // <if expr="not is_ios">
        this.lastFocused = this.lastChanged;
        this.getRestartButton().focus();
        // </if>
      }
    });

    // Remove listeners that were special handling the "Tab" keystroke.
    eventTracker.add(select, 'blur', () => {
      assert(this.lastChanged === select);
      this.lastChanged = null;
      eventTracker.removeAll();
    });
  }

  protected onTextareaChange_() {
    this.showRestartToast(true);
  }

  protected onInputChange_() {
    this.showRestartToast(true);
  }

  // <if expr="not is_ios">
  protected onRestartButtonClick_() {
    FlagsBrowserProxyImpl.getInstance().restartBrowser();
  }
  // </if>

  // <if expr="is_chromeos">
  protected onOsLinkHrefClick_() {
    FlagsBrowserProxyImpl.getInstance().crosUrlFlagsRedirect();
  }
  // </if>

  /**
   * Show the restart toast.
   * @param show Setting to toggle showing / hiding the toast.
   */
  private showRestartToast(show: boolean) {
    this.getRequiredElement('#needs-restart').classList.toggle('show', show);
    // There is no restart button on iOS.
    // <if expr="not is_ios">
    this.getRestartButton().disabled = !show;
    // </if>
    if (show) {
      this.getRequiredElement('#needs-restart').setAttribute('role', 'alert');
    }
  }

  protected shouldShowPromos_(): boolean {
    return this.data.showBetaChannelPromotion ||
        this.data.showDevChannelPromotion;
  }

  protected onSelectedTabIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedTabIndex_ = e.detail.value;
  }

  protected isTabSelected_(index: number): boolean {
    return index === this.selectedTabIndex_;
  }

  // <if expr="not is_ios">
  /**
   * Allows the restart button to jump back to the previously focused experiment
   * in the list instead of going to the top of the page.
   */
  protected onRestartButtonKeydown_(e: KeyboardEvent) {
    if (e.shiftKey && e.key === 'Tab' && this.lastFocused) {
      e.preventDefault();
      this.lastFocused.focus();
    }
  }

  protected onRestartButtonBlur_() {
    this.lastFocused = null;
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'flags-app': FlagsAppElement;
  }
}

// Exported as AppElement to be used by the auto-generated .html.ts file.
export type AppElement = FlagsAppElement;

customElements.define(FlagsAppElement.is, FlagsAppElement);
