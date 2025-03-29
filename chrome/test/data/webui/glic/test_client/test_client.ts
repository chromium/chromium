// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FocusedTabData, GlicBrowserHost, GlicWebClient, Observable, OpenPanelInfo, OpenSettingsOptions, PanelOpeningData, PanelState, TabData, WebClientInitializeError} from '/glic/glic_api/glic_api.js';
import {SettingsPageField, WebClientInitializeErrorReason, WebClientMode} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from '../api_boot.js';

interface PageElementTypes {
  content: HTMLElement;
  status: HTMLElement;
  pageHeader: HTMLElement;
  focusedFavicon: HTMLImageElement;
  focusedUrl: HTMLInputElement;
  contextAccessIndicator: HTMLInputElement;
  panelActiveCheckbox: HTMLInputElement;
  focusedTabLogs: HTMLSpanElement;
  focusedFaviconV2: HTMLImageElement;
  focusedUrlV2: HTMLInputElement;
  contextAccessIndicatorV2: HTMLInputElement;
  focusedTabLogsV2: HTMLSpanElement;
  syncCookiesBn: HTMLButtonElement;
  testLogsBn: HTMLButtonElement;
  syncCookieStatus: HTMLSpanElement;
  getUserProfileInfoBn: HTMLButtonElement;
  getUserProfileInfoStatus: HTMLSpanElement;
  getUserProfileInfoImg: HTMLImageElement;
  changeProfileBn: HTMLButtonElement;
  testPermissionSwitch: HTMLButtonElement;
  openGlicSettings: HTMLButtonElement;
  openGlicSettingsHighlight: HTMLSelectElement;
  microphoneSwitch: HTMLInputElement;
  geolocationSwitch: HTMLInputElement;
  tabContextSwitch: HTMLInputElement;
  newtabbn: HTMLButtonElement;
  reloadpage: HTMLButtonElement;
  getpagecontext: HTMLButtonElement;
  getPageContextResult: HTMLSpanElement;
  getPageContextStatus: HTMLSpanElement;
  URL: HTMLInputElement;
  innerTextCheckbox: HTMLInputElement;
  innerTextBytesLimit: HTMLInputElement;
  viewportScreenshotCheckbox: HTMLInputElement;
  pdfDataCheckbox: HTMLInputElement;
  annotatedPageContentCheckbox: HTMLInputElement;
  screenshotImg: HTMLImageElement;
  faviconImg: HTMLImageElement;
  getlocation: HTMLButtonElement;
  location: HTMLElement;
  locationStatus: HTMLDivElement;
  locationOsErrorUI: HTMLDivElement;
  locationGlicErrorUI: HTMLDivElement;
  openOsLocationSettingsButton: HTMLButtonElement;
  openOsMicrophoneSettings: HTMLButtonElement;
  openOsLocationSettings: HTMLButtonElement;
  openGlicLocationSettingsButton: HTMLButtonElement;
  permissionSelect: HTMLSelectElement;
  enabledSelect: HTMLSelectElement;
  closebn: HTMLButtonElement;
  attachpanelbn: HTMLButtonElement;
  detachpanelbn: HTMLButtonElement;
  refreshbn: HTMLButtonElement;
  navigateWebviewUrl: HTMLInputElement;
  audioCapStop: HTMLButtonElement;
  audioCapStart: HTMLButtonElement;
  audioStatus: HTMLElement;
  mic: HTMLAudioElement;
  audioDuckingOn: HTMLButtonElement;
  audioDuckingOff: HTMLButtonElement;
  desktopScreenshot: HTMLButtonElement;
  desktopScreenshotImg: HTMLImageElement;
  desktopScreenshotErrorReason: HTMLSpanElement;
  createTabInBackground: HTMLInputElement;
  canAttachCheckbox: HTMLInputElement;
  scrollToExactText: HTMLInputElement;
  scrollToTextFragmentTextStart: HTMLInputElement;
  scrollToTextFragmentTextEnd: HTMLInputElement;
  scrollToBn: HTMLButtonElement;
  fileDrop: HTMLDivElement;
  fileDropList: HTMLDivElement;
  showDirectoryPicker: HTMLButtonElement;
  failInitializationCheckbox: HTMLInputElement;
  setExperiment: HTMLButtonElement;
  trialName: HTMLInputElement;
  groupName: HTMLInputElement;
  setExperimentStatus: HTMLSpanElement;
  testClipboardSave: HTMLButtonElement;
  busyWork3s: HTMLButtonElement;
  busyWork8s: HTMLButtonElement;
  contentSizingTest: HTMLElement;
  enableTestSizingMode: HTMLButtonElement;
  disableTestSizingMode: HTMLButtonElement;
  enableDragResizeCheckbox: HTMLInputElement;
  growHeight: HTMLButtonElement;
  resetHeight: HTMLButtonElement;
  dump: HTMLElement;
  fitWindow: HTMLInputElement;
  naturalSizing: HTMLInputElement;
  startMic: HTMLButtonElement;
  successUI: HTMLDivElement;
  localDenialUI: HTMLDivElement;
  osDenialUI: HTMLDivElement;
  openLocalSettingsButton: HTMLButtonElement;
  openOsSettingsButton: HTMLButtonElement;
  osGeolocationPermissionSwitch: HTMLInputElement;
  getOsMicrophonePermissionButton: HTMLButtonElement;
  osMicrophonePermissionResult: HTMLSpanElement;
  osGlicHotkey: HTMLInputElement;
  executeAction: HTMLButtonElement;
  actionProtoEncodedText: HTMLInputElement;
  actionStatus: HTMLSpanElement;
  actionUpdatedContextResult: HTMLSpanElement;
  actionUpdatedScreenshotImg: HTMLImageElement;
  macOsPermissionsFieldset: HTMLFieldSetElement;
  attachmentControlsFieldset: HTMLFieldSetElement;
}

const $: PageElementTypes = new Proxy({}, {
  get(_target: any, prop: string) {
    return document.getElementById(prop);
  },
});

function logMessage(message: string) {
  const d = new Date();
  const hh = String(d.getHours()).padStart(2, '0');
  const mm = String(d.getMinutes()).padStart(2, '0');
  const ss = String(d.getSeconds()).padStart(2, '0');
  const ms = String(d.getMilliseconds()).padStart(3, '0');
  const timeStamp = `${hh}:${mm}:${ss}.${ms}: `;
  $.status.append(
      timeStamp, message.slice(0, 100000), document.createElement('br'));
}

class TestInitFailure extends Error implements WebClientInitializeError {
  reason = WebClientInitializeErrorReason.UNKNOWN;
  readonly reasonType = 'webClientInitialize';
  constructor() {
    super('test-init-failure');
  }
}

class WebClient implements GlicWebClient {
  browser: GlicBrowserHost|undefined;

  async initialize(browser: GlicBrowserHost): Promise<void> {
    if (localStorage.getItem('test-init-failure')) {
      localStorage.removeItem('test-init-failure');
      throw new TestInitFailure();
    }
    this.browser = browser;

    logMessage('initialize called');
    $.pageHeader!.classList.add('connected');

    // Disable sections with unavailable functionality.
    if (this.browser.openOsPermissionSettingsMenu === undefined) {
      logMessage('OS permissions are disabled');
      $.macOsPermissionsFieldset.disabled = true;
    }
    if (this.browser.attachPanel === undefined) {
      logMessage('Attachment controls are disabled (detached-only mode)');
      $.attachmentControlsFieldset.disabled = true;
    }

    const ver = await browser.getChromeVersion();
    logMessage(`Chrome version: ${JSON.stringify(ver)}`);

    const focusedTabState = await this.browser.getFocusedTabState!();
    focusedTabState.subscribe(focusedTabChanged);
    const focusedTabStateV2 = await this.browser.getFocusedTabStateV2!();
    focusedTabStateV2.subscribe(focusedTabChangedV2);

    // Initialize permission switches and subscribe for updates.
    const permissionStates:
        Record<PermissionSwitchName, Observable<boolean>> = {
          microphone: this.browser.getMicrophonePermissionState!(),
          geolocation: this.browser.getLocationPermissionState!(),
          tabContext: this.browser.getTabContextPermissionState!(),
          osGeolocation: this.browser.getOsLocationPermissionState!(),
        };
    for (const permission of Object.keys(permissionStates) as
         PermissionSwitchName[]) {
      const state = permissionStates[permission]!;
      state.subscribe((enabled) => {
        updatePermissionSwitch(permission, enabled);
      });
    }
    browser.canAttachPanel?.().subscribe((canAttach) => {
      $.canAttachCheckbox.checked = canAttach;
    });
    browser.panelActive?.().subscribe((active) => {
      $.panelActiveCheckbox.checked = active;
    });
    browser.isManuallyResizing?.().subscribe((resizing) => {
      logMessage('Manually resizing state changed: ' + resizing);
    });
    if (browser.getOsHotkeyState) {
      const hotkeyState = await browser.getOsHotkeyState();
      hotkeyState.subscribe((data: {hotkey: string}) => {
        $.osGlicHotkey.value = data.hotkey === '' ? 'Not Set' : data.hotkey;
      });
    } else {
      logMessage('getOsHotkeyState not available');
    }
    $.enableDragResizeCheckbox.disabled =
        browser.enableDragResize === undefined;
  }

  async notifyPanelWillOpen(panelOpeningData: PanelOpeningData&PanelState):
      Promise<void|OpenPanelInfo> {
    // Deleting backwards-compatible members coming from PanelState.
    delete (panelOpeningData as Partial<PanelState>).kind;
    delete (panelOpeningData as Partial<PanelState>).windowId;
    logMessage(`notifyPanelWillOpen(${JSON.stringify(panelOpeningData)})`);
    return {
      startingMode: WebClientMode.TEXT,
      resizeParams: {
        width: pickOne([400, 500]),
        height: pickOne([400, 500]),
        options: {durationMs: pickOne([0, 1000])},
      },
    };
  }

  async notifyPanelClosed() {
    logMessage('notifyPanelClosed called');
  }

  async checkResponsive() {
    // Nothing need to be checked on the test client.
  }
}

const client = new WebClient();

// This allows browser tests using this test client to be able to access and
// call the glic API directly (using ExecuteJs and similar methods).
declare global {
  interface Window {
    client: WebClient;
  }
}
window.client = client;

function getBrowser(): GlicBrowserHost|undefined {
  return client?.browser;
}

async function focusedTabChanged(newValue: TabData|undefined) {
  $.focusedUrl.value = '';
  $.focusedFavicon.src = '';
  logMessage(`Focused Tab State Changed: ${JSON.stringify(newValue)}`);
  if (newValue?.url) {
    $.focusedUrl.value = newValue.url;
  }
  if (newValue?.favicon) {
    const fav = await newValue.favicon();
    if (fav) {
      $.focusedFavicon.src = URL.createObjectURL(fav);
    }
  }
}

async function focusedTabChangedV2(focusedTabData: FocusedTabData|undefined) {
  $.focusedUrlV2.value = '';
  $.focusedFaviconV2.src = '';
  $.focusedTabLogsV2.innerText = '';

  if (!focusedTabData) {
    $.focusedTabLogsV2.innerText = 'Focused Tab State Changed: undefined';
    return;
  }

  if (focusedTabData.hasNoFocus) {
    $.focusedTabLogsV2.innerText = `No focus reason: ${
        focusedTabData.hasNoFocus.noFocusReason} active tab url: ${
        focusedTabData.hasNoFocus.tabFocusCandidateData?.url}`;
    return;
  }

  if (focusedTabData.hasFocus) {
    const focusedTab = focusedTabData.hasFocus.tabData;
    $.focusedTabLogsV2.innerText =
        'Focused Tab State Changed: TabData available';
    $.focusedUrlV2.value = focusedTab.url || '';
    if (focusedTab.favicon) {
      const fav = await focusedTab.favicon();
      if (fav) {
        $.focusedFaviconV2.src = URL.createObjectURL(fav);
      }
    }
    return;
  }

  $.focusedTabLogsV2.innerText = 'Focused Tab State Changed: Unknown State';
}

createGlicHostRegistryOnLoad().then((registry) => {
  logMessage('registering web client');
  const params = new URLSearchParams(window.location.search);
  const delayMs = Number(params.get('delay_ms'));
  if (delayMs) {
    setTimeout(() => registry.registerWebClient(client), delayMs);
  } else {
    registry.registerWebClient(client);
  }
});

async function checkMicrophonePermission():
    Promise<'success'|'localDenial'|'osDenial'|'unknown'> {
  try {
    await navigator.mediaDevices.getUserMedia({audio: true});
    return 'success';
  } catch (error) {
    if (error instanceof DOMException && error.name === 'NotAllowedError') {
      // Use the GlicBrowserHost to check the permission state.
      const micPermissionStatus = permissionSwitches['microphone'].checked;
      if (!micPermissionStatus) {
        return 'localDenial';
      } else {
        return 'osDenial';
      }
    } else {
      console.error(error);
    }
    return 'unknown';
  }
}

$.pageHeader.addEventListener('contextmenu', function(event) {
  event.preventDefault();
});

// Test Sizing:
$.enableTestSizingMode.addEventListener('click', () => {
  $.content.setAttribute('hidden', '');
  $.contentSizingTest.removeAttribute('hidden');
  updateSizingMode(true);
});

$.disableTestSizingMode.addEventListener('click', () => {
  $.content.removeAttribute('hidden');
  $.contentSizingTest.setAttribute('hidden', '');
  updateSizingMode(false);
});

$.enableDragResizeCheckbox.addEventListener('change', () => {
  getBrowser()!.enableDragResize!($.enableDragResizeCheckbox.checked);
});

$.growHeight.addEventListener('click', () => {
  const divElement = document.createElement('div');
  divElement.textContent = 'Some Text';
  for (let i = 0; i < 5; i++) {
    $.dump.appendChild(divElement.cloneNode(true));
  }
});

$.resetHeight.addEventListener('click', () => {
  $.dump.innerHTML = '';
});

async function updateSizingMode(inSizingTest: boolean) {
  if (!inSizingTest) {
    document.documentElement.classList.remove('fitWindow');
    return;
  }

  if (await getBrowser()!.shouldFitWindow!()) {
    $.fitWindow.checked = true;
    $.naturalSizing.checked = false;
    document.documentElement.classList.add('fitWindow');
  } else {
    $.fitWindow.checked = false;
    $.naturalSizing.checked = true;
    document.documentElement.classList.remove('fitWindow');
  }
}

// Permissions:

type PermissionSwitchName =
    'microphone'|'geolocation'|'tabContext'|'osGeolocation';
const permissionSwitches: Record<PermissionSwitchName, HTMLInputElement> = {
  microphone: $.microphoneSwitch,
  geolocation: $.geolocationSwitch,
  tabContext: $.tabContextSwitch,
  osGeolocation: $.osGeolocationPermissionSwitch,
};

// Update a permission switch display state.
function updatePermissionSwitch(
    permissionSwitchName: PermissionSwitchName, enabled: boolean) {
  logMessage(
      `Permission ${permissionSwitchName} updated to ${enabled}.`,
  );
  if (!permissionSwitches[permissionSwitchName]) {
    console.error('Permission switch not found: ' + permissionSwitchName);
    return;
  }
  permissionSwitches[permissionSwitchName].checked = enabled;
}

// Listen to permission update requests by the web client user.
$.testPermissionSwitch.addEventListener('click', () => {
  const selectedPermission = $.permissionSelect.value as PermissionSwitchName;
  const isEnabled = $.enabledSelect.value === 'true';
  if (!permissionSwitches[selectedPermission]) {
    console.error('Unknown permission: ' + selectedPermission);
    return;
  }
  if (selectedPermission === 'microphone') {
    getBrowser()!.setMicrophonePermissionState!(isEnabled);
  } else if (selectedPermission === 'geolocation') {
    getBrowser()!.setLocationPermissionState!(isEnabled);
  } else if (selectedPermission === 'tabContext') {
    getBrowser()!.setTabContextPermissionState!(isEnabled);
  }
  logMessage(
      `Setting permission ${selectedPermission} to ${isEnabled}.`,
  );
});

$.openGlicSettings.addEventListener('click', () => {
  const selectedHighlight = $.openGlicSettingsHighlight.value;
  logMessage(`Opening settings. Will highlight field: ${selectedHighlight}`);
  const options: OpenSettingsOptions = {};
  if (selectedHighlight === 'osHotkey') {
    options.highlightField = SettingsPageField.OS_HOTKEY;
  } else if (selectedHighlight === 'osEntrypointToggle') {
    options.highlightField = SettingsPageField.OS_ENTRYPOINT_TOGGLE;
  }
  getBrowser()!.openGlicSettingsPage!(options);
});

$.syncCookiesBn.addEventListener('click', async () => {
  $.syncCookieStatus!.innerText = 'Requesting';
  try {
    await getBrowser()!.refreshSignInCookies!();
    $.syncCookieStatus!.innerText = `Done!`;
  } catch (e) {
    $.syncCookieStatus!.innerText = `Caught error: ${e}`;
  }
});

$.testLogsBn.addEventListener('click', () => {
  getBrowser()?.getMetrics?.().onUserInputSubmitted?.(WebClientMode.TEXT);
  getBrowser()?.getMetrics?.().onResponseStarted?.();
  getBrowser()?.getMetrics?.().onResponseStopped?.();
  getBrowser()?.getMetrics?.().onResponseRated?.(true);
  getBrowser()?.getMetrics?.().onUserInputSubmitted?.(WebClientMode.AUDIO);
  getBrowser()?.getMetrics?.().onResponseStarted?.();
  getBrowser()?.getMetrics?.().onResponseStopped?.();
  getBrowser()?.getMetrics?.().onResponseRated?.(false);
  getBrowser()?.getMetrics?.().onSessionTerminated?.();
});

$.testClipboardSave.addEventListener('click', () => {
  navigator.clipboard.writeText('This is some junk!');
});

$.getUserProfileInfoBn.addEventListener('click', async () => {
  $.getUserProfileInfoStatus.innerText = 'Requesting';
  try {
    const profile = await getBrowser()!.getUserProfileInfo!();
    $.getUserProfileInfoStatus.innerText = `Done: ${JSON.stringify(profile)}`;
    const icon = await profile.avatarIcon();
    if (icon) {
      $.getUserProfileInfoImg.src = URL.createObjectURL(icon);
    }
  } catch (e) {
    $.getUserProfileInfoStatus.innerText = `Caught error: ${e}`;
  }
});

$.changeProfileBn.addEventListener('click', () => {
  getBrowser()!.showProfilePicker!();
});

// Add listeners to demo elements:
$.newtabbn.addEventListener('click', async () => {
  const url = $.URL.value;
  const openInBackground = $.createTabInBackground.checked;
  const tabData = await getBrowser()!.createTab!(url, {openInBackground});
  logMessage(`createTab done: ${JSON.stringify(tabData)}`);
});

$.reloadpage.addEventListener('click', () => {
  location.reload();
});

$.contextAccessIndicator.addEventListener('click', () => {
  getBrowser()!.setContextAccessIndicator!($.contextAccessIndicator.checked);
});

$.contextAccessIndicatorV2.addEventListener('click', () => {
  getBrowser()!.setContextAccessIndicator!($.contextAccessIndicatorV2.checked);
});

$.getpagecontext.addEventListener('click', async () => {
  logMessage('Starting Get Page Context');
  const options: any = {};
  if ($.innerTextCheckbox.checked) {
    options.innerText = true;
  }
  const textLimit = Number.parseInt($.innerTextBytesLimit.value);
  if (!Number.isNaN(textLimit)) {
    options.innerTextBytesLimit = textLimit;
  }
  if ($.viewportScreenshotCheckbox.checked) {
    options.viewportScreenshot = true;
  }
  if ($.pdfDataCheckbox.checked) {
    options.pdfData = true;
  }
  if ($.annotatedPageContentCheckbox.checked) {
    options.annotatedPageContent = true;
  }
  $.faviconImg.src = '';
  $.screenshotImg.src = '';
  try {
    const pageContent =
        await client!.browser!.getContextFromFocusedTab!(options);
    if (pageContent.viewportScreenshot) {
      const blob =
          new Blob([pageContent.viewportScreenshot.data], {type: 'image/jpeg'});
      $.screenshotImg.src = URL.createObjectURL(blob);
    }
    if (pageContent.tabData.favicon) {
      const favicon = await pageContent.tabData.favicon();
      if (favicon) {
        $.faviconImg.src = URL.createObjectURL(favicon);
      }
    }
    if (pageContent.pdfDocumentData) {
      const pdfOrigin = pageContent.pdfDocumentData.origin;
      const pdfSizeLimitExceeded =
          pageContent.pdfDocumentData.pdfSizeLimitExceeded;
      let pdfDataSize = 0;
      if (pageContent.pdfDocumentData.pdfData) {
        pdfDataSize =
            (await readStream(pageContent.pdfDocumentData.pdfData!)).length;
      }
      $.getPageContextResult.innerText =
          `Got ${pdfDataSize} bytes of PDF data(origin = ${
              pdfOrigin}, sizeLimitExceeded = ${pdfSizeLimitExceeded})`;
    }
    if (pageContent.annotatedPageData &&
        pageContent.annotatedPageData.annotatedPageContent) {
      const annotatedPageDataSize =
          (await readStream(pageContent.annotatedPageData.annotatedPageContent))
              .length;
      $.getPageContextResult.innerText =
          `Annotated page content data length: ${annotatedPageDataSize}`;
    }
    $.getPageContextStatus.innerText = 'Finished Get Page Context.';
    $.getPageContextResult.innerText =
        `Returned data: ${JSON.stringify(pageContent, null, 2)}`;
  } catch (error) {
    $.getPageContextStatus.innerText = `Error getting page context: ${error}`;
  }
});

$.executeAction.addEventListener('click', async () => {
  logMessage('Starting Execute Action');

  // The action proto is expected to be a BrowserAction proto, which is binary
  // serialized and then base64 encoded.
  const protoByteString = atob($.actionProtoEncodedText.value);
  const protoBytes = new Uint8Array(protoByteString.length);
  for (let i = 0; i < protoByteString.length; i++) {
    protoBytes[i] = protoByteString.charCodeAt(i);
  }

  const params: any = {
    actionProto: protoBytes.buffer,
    tabContextOptions: {annotatedPageContent: true, viewportScreenshot: true},
  };

  $.actionUpdatedContextResult.innerText = '';
  $.actionUpdatedScreenshotImg.src = '';
  try {
    const actionResult = await client!.browser!.actInFocusedTab!(params);
    const pageContent = actionResult.tabContextResult;
    if (pageContent) {
      if (pageContent.viewportScreenshot) {
        const blob = new Blob(
            [pageContent.viewportScreenshot.data], {type: 'image/jpeg'});
        $.actionUpdatedScreenshotImg.src = URL.createObjectURL(blob);
      }
      if (pageContent.annotatedPageData &&
          pageContent.annotatedPageData.annotatedPageContent) {
        const annotatedPageDataSize =
            (await readStream(
                 pageContent.annotatedPageData.annotatedPageContent))
                .length;
        $.actionUpdatedContextResult.innerText =
            `Annotated page content data length: ${annotatedPageDataSize}`;
      }
    }
    $.actionStatus.innerText = 'Finished Execute Action.';
    $.actionUpdatedContextResult.innerText +=
        `Returned data: ${JSON.stringify(pageContent, null, 2)}`;
  } catch (error) {
    $.actionStatus.innerText = `Error in Execute Action: ${error}`;
  }
});

$.getlocation.addEventListener('click', async () => {
  if (navigator.geolocation) {
    try {
      $.locationStatus.innerText = 'Requesting geolocation...';
      const position =
          await new Promise<GeolocationPosition>((resolve, reject) => {
            navigator.geolocation.getCurrentPosition(resolve, reject);
          });
      const latitude = position.coords.latitude;
      const longitude = position.coords.longitude;
      const accuracy = position.coords.accuracy;

      $.location.innerHTML = `
          Latitude: ${latitude}<br>
          Longitude: ${longitude}<br>
          Accuracy: ${accuracy} meters
        `;
      $.locationStatus.innerText = `Location Received.`;
    } catch (error) {
      $.locationStatus.innerText = `Error: ${error}`;
      $.location.innerHTML = ``;
      if (error instanceof GeolocationPositionError) {
        if (error.code === 1) {
          $.locationStatus.innerText = `Permission Denied.`;
          if (!permissionSwitches['osGeolocation'].checked) {
            $.locationOsErrorUI.style.display = 'block';
          } else if (!permissionSwitches['geolocation'].checked) {
            $.locationGlicErrorUI.style.display = 'block';
          }
        }
      }
    }
  } else {
    $.location.innerHTML = 'Geolocation is not supported by this browser.';
  }
});

$.closebn.addEventListener('click', () => {
  getBrowser()!.closePanel!();
});
$.attachpanelbn.addEventListener('click', () => {
  getBrowser()!.attachPanel!();
});
$.detachpanelbn.addEventListener('click', () => {
  getBrowser()!.detachPanel!();
});
$.refreshbn.addEventListener('click', () => {
  location.reload();
});
$.navigateWebviewUrl.addEventListener('keyup', ({key}) => {
  if (key === 'Enter') {
    window.location.href = $.navigateWebviewUrl.value;
  }
});

class FileListUpdater {
  constructor() {
    $.fileDropList.replaceChildren();
  }
  appendEntry(text: string) {
    const el = document.createElement('li');
    el.replaceChildren(text);
    $.fileDropList.appendChild(el);
  }

  async addFile(file: File) {
    try {
      const text = await file.text();
      this.appendEntry(`${file.name}: ${text.length} chars read`);
    } catch (e) {
      this.appendEntry(`${file.name}: Error reading file: ${e}`);
    }
  }
  async addHandle(handle: FileSystemHandle) {
    if (handle.kind === 'directory') {
      const dir = handle as FileSystemDirectoryHandle;
      this.appendEntry(`Dropped directory: ${dir.name}`);
      for await (const [_, h] of dir.entries()) {
        this.addHandle(h);
      }
    } else if (handle.kind === 'file') {
      const file = handle as FileSystemFileHandle;
      this.addFile(await file.getFile());
    }
  }
  async addItem(item: DataTransferItem) {
    const handle =
        (await (item as any).getAsFileSystemHandle()) as FileSystemHandle;
    if (handle) {
      this.addHandle(handle);
    } else {
      const file = item.getAsFile();
      if (file) {
        this.addFile(file);
      }
    }
  }
}

// When files or directories are dropped, log the contents to `fileDropList`.
// This confirms that the web client has file access.
$.fileDrop.addEventListener('drop', (e: DragEvent) => {
  e.preventDefault();
  if (!e.dataTransfer) {
    return;
  }

  const updater = new FileListUpdater();

  if (e.dataTransfer.items) {
    [...e.dataTransfer.items].forEach((item) => {
      updater.addItem(item);
    });
  } else {
    [...e.dataTransfer.files].forEach((file) => {
      updater.addFile(file);
    });
  }
});
$.fileDrop.addEventListener('dragover', (e) => {
  e.preventDefault();
});
$.showDirectoryPicker.addEventListener('click', async () => {
  const updater = new FileListUpdater();
  try {
    const handle = (await (window as any).showDirectoryPicker()) as
        FileSystemDirectoryHandle;
    updater.addHandle(handle);
  } catch (e) {
    updater.appendEntry(`Error: ${e}`);
  }
});

$.audioDuckingOn.addEventListener('click', () => {
  getBrowser()!.setAudioDucking!(true);
});

$.audioDuckingOff.addEventListener('click', () => {
  getBrowser()!.setAudioDucking!(false);
});

$.scrollToBn.addEventListener('click', async () => {
  if (!(getBrowser()!.scrollTo)) {
    logMessage(
        `scrollTo is not enabled. Run with --enable-features=GlicScrollTo.`);
    return;
  }

  try {
    const exactText = $.scrollToExactText.value;
    if (exactText) {
      logMessage(`scrollTo called with "${exactText}"`);
      await getBrowser()!.scrollTo!({
        selector: {exactText: {text: exactText}},
        highlight: true,
      });
      logMessage('scrollTo succeeded!');
      return;
    }

    const textStart = $.scrollToTextFragmentTextStart.value;
    const textEnd = $.scrollToTextFragmentTextEnd.value;
    if (textStart && textEnd) {
      logMessage(`scrollTo called with text fragment: {textStart: "${
          textStart}", textEnd: "${textEnd}"}`);
      await getBrowser()!.scrollTo!
          ({selector: {textFragment: {textStart, textEnd}}});
      logMessage('scrollTo succeeded!');
      return;
    }

    logMessage('scrollTo: no selector specified');
  } catch (error) {
    logMessage(`scrollTo failed: ${error}`);
  }
});

// Hang web client for <workTimeMs> amount of time.
function busyWork(workTimeMs: number) {
  const end = performance.now() + workTimeMs;
  while (performance.now() < end) {
    // mock busy work to test the web client unresponsive handling.
  }
}

$.busyWork3s.addEventListener('click', () => {
  busyWork(3000);
});

$.busyWork8s.addEventListener('click', () => {
  busyWork(8000);
});


class AudioCapture {
  recordedData: Blob[] = [];
  recorder: MediaRecorder|undefined;
  constructor() {}

  async start() {
    if (this.recorder) {
      return;
    }
    try {
      $.audioStatus.innerText = 'Starting Recording...';
      const stream = await navigator.mediaDevices.getUserMedia({audio: true});
      this.recorder = new MediaRecorder(stream, {mimeType: 'audio/webm'});
      let stopped = false;
      window.setInterval(() => {
        if (!stopped) {
          this.recorder!.requestData();
        }
      }, 100);
      this.recorder.addEventListener('dataavailable', (event: BlobEvent) => {
        this.recordedData.push(event.data);
      });
      this.recorder.addEventListener('stop', () => {
        stopped = true;
        $.audioStatus.innerText = 'Recording Stopped';
        const blob = new Blob(this.recordedData, {type: 'audio/webm'});
        $.mic.src = URL.createObjectURL(blob);
      });
      this.recorder.start();
      $.audioStatus.innerText = 'Recording...';
    } catch (error) {
      $.audioStatus.innerText = `Caught error: ${error}`;
    }
  }

  stop() {
    if (!this.recorder) {
      return;
    }
    $.mic.play();
    this.recorder.stop();
    this.recorder = undefined;
  }
}
const audioCapture = new AudioCapture();

window.addEventListener('load', () => {
  $.audioCapStop.addEventListener('click', () => {
    audioCapture.stop();
  });
  $.audioCapStart.addEventListener('click', () => {
    audioCapture.start();
  });
  $.desktopScreenshot.addEventListener('click', async () => {
    logMessage('Requesting desktop screenshot...');
    try {
      const screenshot = await getBrowser()!.captureScreenshot!();
      if (screenshot) {
        const blob = new Blob([screenshot.data], {type: 'image/jpeg'});
        $.desktopScreenshotImg.src = URL.createObjectURL(blob);
        $.desktopScreenshotErrorReason!.innerText =
            'Desktop screenshot captured.';
      } else {
        $.desktopScreenshotErrorReason!.innerText =
            'Failed to capture desktop screenshot.';
      }
    } catch (error) {
      $.desktopScreenshotErrorReason!.innerText = `Caught error: ${error}`;
    }
  });
  $.setExperiment.addEventListener('click', async () => {
    const trialName = $.trialName.value;
    const groupName = $.groupName.value;
    $.setExperimentStatus!.innerText +=
        `\nSetting experiment: ${trialName} ${groupName}`;
    await getBrowser()!.setSyntheticExperimentState!(trialName, groupName);
    $.setExperimentStatus!.innerText += '\nExperiment State Set.';
  });
  $.startMic.addEventListener('click', async () => {
    const permissionResult = await checkMicrophonePermission();

    $.startMic.style.display = 'none';

    if (permissionResult === 'success') {
      $.successUI.style.display = 'block';
    } else if (permissionResult === 'localDenial') {
      $.localDenialUI.style.display = 'block';
    } else if (permissionResult === 'osDenial') {
      $.osDenialUI.style.display = 'block';
    }
  });

  $.openLocalSettingsButton.addEventListener('click', () => {
    getBrowser()!.openGlicSettingsPage!();
  });
  $.openOsSettingsButton.addEventListener('click', () => {
    getBrowser()!.openOsPermissionSettingsMenu!('media');
  });
  $.openOsLocationSettingsButton.addEventListener('click', () => {
    getBrowser()!.openOsPermissionSettingsMenu!('geolocation');
  });
  $.openOsLocationSettings.addEventListener('click', () => {
    getBrowser()!.openOsPermissionSettingsMenu!('geolocation');
  });
  $.openOsMicrophoneSettings.addEventListener('click', () => {
    getBrowser()!.openOsPermissionSettingsMenu!('media');
  });
  $.openGlicLocationSettingsButton.addEventListener('click', () => {
    getBrowser()!.openGlicSettingsPage!();
  });
  $.getOsMicrophonePermissionButton.addEventListener('click', async () => {
    const permission = await getBrowser()!.getOsMicrophonePermissionStatus!();
    $.osMicrophonePermissionResult.textContent =
        `OS Microphone Permission: ${permission}`;
  });
});

function readStream(stream: ReadableStream<Uint8Array>): Promise<Uint8Array> {
  return new Response(stream).bytes();
}

function pickOne(choices: any[]): any {
  return choices[Math.floor(Math.random() * choices.length)];
}

$.failInitializationCheckbox.addEventListener('click', () => {
  if ($.failInitializationCheckbox.checked) {
    localStorage.setItem('test-init-failure', 'true');
  } else {
    localStorage.removeItem('test-init-failure');
  }
});
