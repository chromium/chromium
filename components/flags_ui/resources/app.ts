// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

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

interface Tab {
  tabEl: HTMLElement;
  panelEl: HTMLElement;
}


/**
 * Handles in page searching. Matches against the experiment flag name.
 */
class FlagSearch {
  private flagsAppElement: FlagsAppElement;
  private initialized: boolean = false;
  private noMatchMsg: NodeListOf<HTMLElement>;
  private searchIntervalId: number|null = null;
  private searchBox: HTMLInputElement;
  // Delay in ms following a keypress, before a search is made.
  private searchDebounceDelayMs: number = 150;

  constructor(el: FlagsAppElement) {
    this.flagsAppElement = el;
    this.searchBox =
        this.flagsAppElement.getRequiredElement<HTMLInputElement>('#search');
    this.noMatchMsg = this.flagsAppElement.shadowRoot!.querySelectorAll(
        '.tab-content .no-match');
  }

  /**
   * Initialises the in page search. Adds searchbox listeners and
   * collates the text elements used for string matching.
   */
  init() {
    if (this.initialized) {
      return;
    }

    window.addEventListener('keyup', e => {
      // Check for an active textarea inside a <flags-experiment>.
      const activeElement = getDeepActiveElement();
      if (activeElement && activeElement.nodeName === 'TEXTAREA') {
        return;
      }
      switch (e.key) {
        case '/':
          this.searchBox.focus();
          break;
        case 'Escape':
          this.searchBox.blur();
          break;
      }
    });
    this.searchBox.focus();
    this.initialized = true;
  }

  /**
   * Clears a search showing all experiments.
   */
  clearSearch() {
    this.searchBox.value = '';
    this.doSearch();
    this.searchBox.focus();
  }

  /**
   * Goes through all experiment text and highlights the relevant matches.
   * Only the first instance of a match in each experiment text block is
   * highlighted. This prevents the sea of yellow that happens using the
   * global find in page search.
   * @param experiments The list of elements to search on and highlight.
   * @param searchTerm The query to search for.
   * @return The number of matches found.
   */
  private async highlightAllMatches(
      experiments: NodeListOf<FlagsExperimentElement>,
      searchTerm: string): Promise<number> {
    let matches = 0;
    // Not using for..of with async/await to spawn all searching in parallel.
    await Promise.all(Array.from(experiments).map(async (experiment) => {
      const hasMatch = await experiment.match(searchTerm);
      matches += hasMatch ? 1 : 0;
      experiment.classList.toggle('hidden', !hasMatch);
    }));
    return matches;
  }

  /**
   * Performs a search against the experiment title, description, platforms and
   * permalink text.
   */
  async doSearch() {
    const searchTerm = this.searchBox.value.trim().toLowerCase();

    this.flagsAppElement.classList.toggle('searching', Boolean(searchTerm));

    // Available experiments
    const availableExperiments =
        this.flagsAppElement.shadowRoot!
            .querySelectorAll<FlagsExperimentElement>(
                '#tab-content-available flags-experiment');
    assert(this.noMatchMsg[0]);
    this.noMatchMsg[0].classList.toggle(
        'hidden',
        await this.highlightAllMatches(availableExperiments, searchTerm) > 0);

    // <if expr="not is_ios">
    // Unavailable experiments, which are undefined on iOS.
    const unavailableExperiments =
        this.flagsAppElement.shadowRoot!
            .querySelectorAll<FlagsExperimentElement>(
                '#tab-content-unavailable flags-experiment');
    assert(this.noMatchMsg[1]);
    this.noMatchMsg[1].classList.toggle(
        'hidden',
        await this.highlightAllMatches(unavailableExperiments, searchTerm) > 0);
    // </if>
    await this.announceSearchResults();
    this.flagsAppElement.dispatchEvent(
        new Event('search-finished-for-testing', {
          bubbles: true,
          composed: true,
        }));

    this.searchIntervalId = null;
  }

  private announceSearchResults(): Promise<void> {
    const searchTerm = this.searchBox.value.trim().toLowerCase();
    if (!searchTerm) {
      return Promise.resolve();
    }

    const selectedTab = this.flagsAppElement.getTabs().find(
        tab => tab.panelEl.classList.contains('selected'))!;
    const selectedTabId = selectedTab.panelEl.id;
    const queryString = `#${selectedTabId} flags-experiment:not(.hidden)`;
    const total =
        this.flagsAppElement.shadowRoot!.querySelectorAll(queryString).length;
    if (total) {
      return this.flagsAppElement.announceStatus(
          total === 1 ?
              loadTimeData.getStringF('searchResultsSingular', searchTerm) :
              loadTimeData.getStringF(
                  'searchResultsPlural', total, searchTerm));
    }
    return Promise.resolve();
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
    };
  }

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
    this.flagSearch.init();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    this.showRestartToast(this.data.needsRestart);
    this.highlightReferencedFlag();
    this.featuresResolver.resolve();
  }

  getTabs(): Tab[] {
    return [
      {
        tabEl: this.getRequiredElement('#tab-available'),
        panelEl: this.getRequiredElement('#tab-content-available'),
      },
      // <if expr="not is_ios">
      {
        tabEl: this.getRequiredElement('#tab-unavailable'),
        panelEl: this.getRequiredElement('#tab-content-unavailable'),
      },
      // </if>
    ];
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
    // Update the highlighted flag when the hash changes.
    window.addEventListener('hashchange', () => this.highlightReferencedFlag);

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
      this.getRequiredElement<HTMLInputElement>('#search').placeholder =
          loadTimeData.getString('deprecatedSearchPlaceholder');
      for (const element of this.shadowRoot!.querySelectorAll('.no-match')) {
        element.textContent = loadTimeData.getString('deprecatedNoResults');
      }
    }
    // </if>
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

  /**
   * Toggles necessary attributes to display selected tab.
   */
  private selectTab(selectedTabEl: HTMLElement) {
    for (const tab of this.getTabs()) {
      const isSelectedTab = tab.tabEl === selectedTabEl;
      tab.tabEl.classList.toggle('selected', isSelectedTab);
      tab.tabEl.setAttribute('aria-selected', String(isSelectedTab));
      tab.panelEl.classList.toggle('selected', isSelectedTab);
    }
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
  private highlightReferencedFlag() {
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
      this.selectTab(this.getRequiredElement('#tab-unavailable'));
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

  /** Reset all flags to their default values and refresh the UI. */
  protected async onResetAllClick_(e: Event) {
    this.lastChanged = e.target as HTMLElement;
    FlagsBrowserProxyImpl.getInstance().resetAllFlags();
    this.announceStatus(loadTimeData.getString('reset-acknowledged'));
    this.showRestartToast(true);

    await this.requestExperimentalFeaturesData();
    await this.updateComplete;

    assert(this.flagSearch);
    this.flagSearch.clearSearch();
  }

  protected onSearchInput_() {
    assert(this.flagSearch);
    this.flagSearch.debounceSearch();
  }

  protected onClearSearchClick_() {
    assert(this.flagSearch);
    this.flagSearch.clearSearch();
  }

  protected onSelectChange_(e: Event) {
    const select = e.composedPath()[0];
    assert(select instanceof HTMLSelectElement);
    this.lastChanged = select;
    this.showRestartToast(true);

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

  protected onTabClick_(e: Event) {
    e.preventDefault();
    this.selectTab(e.target as HTMLElement);
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
