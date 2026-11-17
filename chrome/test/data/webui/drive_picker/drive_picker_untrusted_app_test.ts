// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://drive-picker-host/strings.m.js';
import 'chrome-untrusted://drive-picker-host/app.js';

import {PromiseResolver} from '//resources/js/promise_resolver.js';
import type {ProtoWrapper} from '//resources/mojo/mojo/public/mojom/base/proto_wrapper.mojom-webui.js';
import {HANDSHAKE_TIMEOUT_MS} from 'chrome-untrusted://drive-picker-host/app.js';
import type {DrivePickerHostUntrustedAppElement} from 'chrome-untrusted://drive-picker-host/app.js';
import {DrivePickerApiProxyImpl} from 'chrome-untrusted://drive-picker-host/drive_picker_api_proxy.js';
import type {DrivePickerApiProxy, GooglePicker, GooglePickerBuilder, GooglePickerInstance, GooglePickerResponse, GooglePickerView} from 'chrome-untrusted://drive-picker-host/drive_picker_api_proxy.js';
import {browserProxyFactory} from 'chrome-untrusted://drive-picker-host/drive_picker_host_untrusted.mojom-webui.js';
import type {PageHandlerInterface, PageRemote} from 'chrome-untrusted://drive-picker-host/drive_picker_host_untrusted.mojom-webui.js';
import {DrivePickerError, DrivePickerResultHandlerReceiver} from 'chrome-untrusted://drive-picker-host/drive_picker_result_handler.mojom-webui.js';
import type {DriveFile, DrivePickerResultHandlerInterface} from 'chrome-untrusted://drive-picker-host/drive_picker_result_handler.mojom-webui.js';
import type {SanitizedDriveFile} from 'chrome-untrusted://drive-picker-host/sanitizer.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

class TestPageHandler extends TestBrowserProxy implements PageHandlerInterface {
  constructor() {
    super([
      'onConsentKitIframeMessage',
      'onConsentKitPrivacyFlowResult',
      'onConsentKitError',
      'onShowErrorDialog',
      'onHideErrorDialog',
    ]);
  }

  onConsentKitIframeMessage(messageWrapper: ProtoWrapper) {
    this.methodCalled('onConsentKitIframeMessage', messageWrapper);
  }

  onConsentKitPrivacyFlowResult(resultWrapper: ProtoWrapper) {
    this.methodCalled('onConsentKitPrivacyFlowResult', resultWrapper);
  }

  onConsentKitError(errorMessage: string) {
    this.methodCalled('onConsentKitError', errorMessage);
  }

  onShowErrorDialog() {
    this.methodCalled('onShowErrorDialog');
  }

  onHideErrorDialog() {
    this.methodCalled('onHideErrorDialog');
  }
}

class TestDrivePickerApiProxy extends TestBrowserProxy implements
    DrivePickerApiProxy {
  pickerResolver = new PromiseResolver<SanitizedDriveFile[]|'CANCEL'>();

  constructor() {
    super(['showPicker']);
  }

  showPicker(oauthToken: string, apiKey: string, appId: string, isGlic = false):
      Promise<SanitizedDriveFile[]|'CANCEL'> {
    this.methodCalled('showPicker', [oauthToken, apiKey, appId, isGlic]);
    return this.pickerResolver.promise;
  }
}

class TestResultHandler extends TestBrowserProxy implements
    DrivePickerResultHandlerInterface {
  receiver = new DrivePickerResultHandlerReceiver(this);

  constructor() {
    super(['onSelection', 'onCancel', 'onError']);
  }

  onSelection(files: DriveFile[]) {
    this.methodCalled('onSelection', files);
  }

  onCancel() {
    this.methodCalled('onCancel');
  }

  onError(error: DrivePickerError) {
    this.methodCalled('onError', error);
  }
}

suite('DrivePickerHostUntrustedAppTest', function() {
  let app: DrivePickerHostUntrustedAppElement;
  let handler: TestPageHandler;
  let apiProxy: TestDrivePickerApiProxy;
  let callbackRouterRemote: PageRemote;
  let mockTimer: MockTimer|null = null;

  setup(async function() {
    if (window.trustedTypes) {
      document.body.innerHTML = window.trustedTypes.emptyHTML;
    } else {
      document.body.innerHTML = '';
    }
    document.body.classList.remove('glic-picker');
    handler = new TestPageHandler();
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    apiProxy = new TestDrivePickerApiProxy();
    DrivePickerApiProxyImpl.setInstance(apiProxy);
    callbackRouterRemote = remote;
    app = document.createElement('drive-picker-host-untrusted-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  teardown(function() {
    mockTimer?.uninstall();
    mockTimer = null;
    document.body.classList.remove('glic-picker');
  });

  test('AppIsAttached', function() {
    assertTrue(app.isConnected);
  });

  test('LoadsConsentKitUrlIframe', async function() {
    const testUrl = 'https://consent.google.com/primitive?hl=en';

    callbackRouterRemote.loadConsentKitUrl(testUrl);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(!!app.shadowRoot);
    const container = app.shadowRoot.querySelector('#picker-container');
    assertTrue(!!container);
    if (!container) {
      return;
    }
    const iframe = container.querySelector('iframe');
    assertTrue(!!iframe);
    if (!iframe) {
      return;
    }
    assertEquals(testUrl, iframe.src);
  });

  test('TryAgainNotifiesBrowserAndReloadsConsentKit', async function() {
    const testUrl = 'https://consent.google.com/primitive?hl=en';
    mockTimer = new MockTimer();
    mockTimer.install();
    callbackRouterRemote.loadConsentKitUrl(testUrl);
    await callbackRouterRemote.$.flushForTesting();

    // ConsentKit never sends the handshake, so the error screen is shown.
    mockTimer.tick(HANDSHAKE_TIMEOUT_MS);
    await app.updateComplete;
    assertEquals(1, handler.getCallCount('onShowErrorDialog'));
    const container =
        app.shadowRoot.querySelector<HTMLElement>('#picker-container');
    assertTrue(!!container);
    assertTrue(container.hidden);
    const oldIframe = container.querySelector('iframe');
    assertTrue(!!oldIframe);
    const tryAgainButton = app.shadowRoot.querySelector<HTMLElement>(
        '#error-screen .action-button');
    assertTrue(!!tryAgainButton);

    tryAgainButton.click();
    await app.updateComplete;

    // The browser is told to leave the error size and ConsentKit is reloaded.
    assertEquals(1, handler.getCallCount('onHideErrorDialog'));
    assertFalse(!!app.shadowRoot.querySelector('#error-screen'));
    assertFalse(container.hidden);
    const newIframe = container.querySelector('iframe');
    assertTrue(!!newIframe);
    assertNotEquals(oldIframe, newIframe);
    assertEquals(testUrl, newIframe.src);
  });

  test('ShowDrivePickerTogglesGlicClassAndForwardsSelection', async function() {
    const glicResultHandler = new TestResultHandler();
    callbackRouterRemote.showDrivePicker(
        glicResultHandler.receiver.$.bindNewPipeAndPassRemote(), {
          oauthToken: 'token-1',
          apiKey: 'key-1',
          appId: '',
          isGlic: true,
        });
    const glicArgs = await apiProxy.whenCalled('showPicker');
    assertDeepEquals(['token-1', 'key-1', '', true], glicArgs);
    assertTrue(document.body.classList.contains('glic-picker'));

    const sampleFile: SanitizedDriveFile = {
      id: 'file-1',
      mimeType: 'application/pdf',
      name: 'doc.pdf',
      type: 'document',
      sizeBytes: 42n,
      resourceKey: null,
      iconUrl: null,
      thumbnailUrl: null,
    };
    apiProxy.pickerResolver.resolve([sampleFile]);
    const selectedFiles = await glicResultHandler.whenCalled('onSelection');
    assertEquals(1, selectedFiles.length);
    assertEquals('file-1', selectedFiles[0].id);

    // A subsequent non-Glic call removes the `glic-picker` class.
    apiProxy.resetResolver('showPicker');
    apiProxy.pickerResolver = new PromiseResolver();
    const defaultResultHandler = new TestResultHandler();
    callbackRouterRemote.showDrivePicker(
        defaultResultHandler.receiver.$.bindNewPipeAndPassRemote(), {
          oauthToken: 'token-2',
          apiKey: 'key-2',
          appId: 'app-2',
          isGlic: false,
        });
    const defaultArgs = await apiProxy.whenCalled('showPicker');
    assertDeepEquals(['token-2', 'key-2', 'app-2', false], defaultArgs);
    assertFalse(document.body.classList.contains('glic-picker'));

    apiProxy.pickerResolver.resolve('CANCEL');
    await defaultResultHandler.whenCalled('onCancel');
  });

  test('ShowDrivePickerRejectsConcurrentRequestWhileActive', async function() {
    const firstHandler = new TestResultHandler();
    callbackRouterRemote.showDrivePicker(
        firstHandler.receiver.$.bindNewPipeAndPassRemote(), {
          oauthToken: 'token-1',
          apiKey: 'key-1',
          appId: '',
          isGlic: true,
        });
    await apiProxy.whenCalled('showPicker');
    assertTrue(document.body.classList.contains('glic-picker'));

    // A second overlapping call is rejected with kAlreadyActive without
    // clobbering `body.glic-picker` or the first request's resultHandler.
    const secondHandler = new TestResultHandler();
    callbackRouterRemote.showDrivePicker(
        secondHandler.receiver.$.bindNewPipeAndPassRemote(), {
          oauthToken: 'token-2',
          apiKey: 'key-2',
          appId: 'app-2',
          isGlic: false,
        });
    const error = await secondHandler.whenCalled('onError');
    assertEquals(DrivePickerError.kAlreadyActive, error);
    assertTrue(document.body.classList.contains('glic-picker'));

    apiProxy.pickerResolver.resolve('CANCEL');
    await firstHandler.whenCalled('onCancel');
  });

  test(
      'DrivePickerApiProxyBuildsGlicTabsAndHandlesPartialSanitization',
      async function() {
        interface RecordedView {
          viewId: string;
          includeFolders?: boolean;
          ownedByMe?: boolean;
          enableDrives?: boolean;
          starred?: boolean;
          mode?: string;
          label?: string;
        }

        let capturedViews: RecordedView[] = [];
        let capturedMaxItems: number|null = null;
        let capturedAppId: string|null = null;
        let capturedCallback: (data: GooglePickerResponse) => void = () => {
          assertTrue(false, 'Picker callback was not set');
        };

        class FakeDocsView implements GooglePickerView {
          record: RecordedView;
          constructor(viewId: string) {
            this.record = {viewId};
          }
          setMimeTypes(_mimeTypes: string) {
            return this;
          }
          setIncludeFolders(include: boolean) {
            this.record.includeFolders = include;
            return this;
          }
          setOwnedByMe(ownedByMe: boolean) {
            this.record.ownedByMe = ownedByMe;
            return this;
          }
          setEnableDrives(enable: boolean) {
            this.record.enableDrives = enable;
            return this;
          }
          setStarred(starred: boolean) {
            this.record.starred = starred;
            return this;
          }
          setMode(mode: string) {
            this.record.mode = mode;
            return this;
          }
          setLabel(label: string) {
            this.record.label = label;
            return this;
          }
        }

        class FakePickerBuilder implements GooglePickerBuilder {
          addView(view: GooglePickerView) {
            capturedViews.push((view as unknown as FakeDocsView).record);
            return this;
          }
          // eslint-disable-next-line @typescript-eslint/naming-convention
          setOAuthToken(_token: string) {
            return this;
          }
          setDeveloperKey(_key: string) {
            return this;
          }
          setAppId(appId: string) {
            capturedAppId = appId;
            return this;
          }
          setOrigin(_origin: string) {
            return this;
          }
          setSize(_width: number, _height: number) {
            return this;
          }
          setMaxItems(max: number) {
            capturedMaxItems = max;
            return this;
          }
          enableFeature(_feature: string) {
            return this;
          }
          setCallback(callback: (data: GooglePickerResponse) => void) {
            capturedCallback = callback;
            return this;
          }
          build(): GooglePickerInstance {
            return {setVisible: () => {}};
          }
        }

        window.gapi = {
          load: (_api: string, options: {callback: () => void}) => {
            options.callback();
          },
        };
        const fakePicker: GooglePicker = {
          DocsView: FakeDocsView,
          PickerBuilder: FakePickerBuilder,
          DocsViewMode: {GRID: 'grid', LIST: 'list'},
          ViewId: {DOCS: 'all'},
          Feature: {MULTISELECT_ENABLED: 'multiselectEnabled'},
          Action: {PICKED: 'picked', CANCEL: 'cancel'},
          Response: {ACTION: 'action', DOCUMENTS: 'docs'},
          Document: {
            ID: 'id',
            MIME_TYPE: 'mimeType',
            NAME: 'name',
            TYPE: 'type',
            ICON_URL: 'iconUrl',
          },
          Type: {
            DOCUMENT: 'document',
            PHOTO: 'photo',
            VIDEO: 'video',
          },
        };
        (window as unknown as {google: {picker: GooglePicker}}).google = {
          picker: fakePicker,
        };

        const realProxy = new DrivePickerApiProxyImpl();

        // 1. Glic mode: 5 labeled tabs, setMaxItems(10), omitted setAppId, and
        // partial sanitization retains valid document when another fails.
        const glicPromise = realProxy.showPicker('tok', 'key', '', true);
        await microtasksFinished();
        assertEquals(5, capturedViews.length);
        assertEquals('list', capturedViews[0]!.mode);
        assertTrue(!!capturedViews[0]!.label);
        assertEquals(true, capturedViews[1]!.includeFolders);
        assertEquals(true, capturedViews[1]!.ownedByMe);
        assertEquals(true, capturedViews[2]!.includeFolders);
        assertEquals(true, capturedViews[2]!.enableDrives);
        assertEquals(true, capturedViews[3]!.includeFolders);
        assertEquals(false, capturedViews[3]!.ownedByMe);
        assertEquals(true, capturedViews[4]!.includeFolders);
        assertEquals(true, capturedViews[4]!.starred);
        assertEquals(10, capturedMaxItems);
        assertEquals(null, capturedAppId);

        capturedCallback({
          action: 'picked',
          docs: [
            {
              id: 'valid-file-1',
              mimeType: 'application/pdf',
              name: 'valid.pdf',
              type: 'document',
              sizeBytes: 100,
            },
            {
              id: 'invalid id with spaces',
              mimeType: 'application/pdf',
              name: 'bad.pdf',
              type: 'document',
              sizeBytes: 100,
            },
          ],
        });
        const glicResult = await glicPromise;
        assertTrue(Array.isArray(glicResult));
        assertEquals(1, glicResult.length);
        assertEquals('valid-file-1', glicResult[0]!.id);

        // 2. Non-Glic mode: 1 unlabeled tab, setAppId called, and fail-fast on
        // invalid document in multi-select.
        capturedViews = [];
        capturedMaxItems = null;
        capturedAppId = null;
        const nonGlicPromise =
            realProxy.showPicker('tok', 'key', '12345', false);
        await microtasksFinished();
        assertEquals(1, capturedViews.length);
        assertEquals(undefined, capturedViews[0]!.label);
        assertEquals(null, capturedMaxItems);
        assertEquals('12345', capturedAppId);
        capturedCallback({
          action: 'picked',
          docs: [
            {
              id: 'valid-file-1',
              mimeType: 'application/pdf',
              name: 'valid.pdf',
              type: 'document',
              sizeBytes: 100,
            },
            {
              id: 'invalid id with spaces',
              mimeType: 'application/pdf',
              name: 'bad.pdf',
              type: 'document',
              sizeBytes: 100,
            },
          ],
        });
        let nonGlicError: unknown = null;
        try {
          await nonGlicPromise;
        } catch (e) {
          nonGlicError = e;
        }
        assertEquals(DrivePickerError.kInvalidFileId, nonGlicError);

        // 3. Empty `docs` array rejects with kInvalidResponse.
        const emptyDocsPromise = realProxy.showPicker('tok', 'key', '', true);
        await microtasksFinished();
        capturedCallback({action: 'picked', docs: []});
        let emptyError: unknown = null;
        try {
          await emptyDocsPromise;
        } catch (e) {
          emptyError = e;
        }
        assertEquals(DrivePickerError.kInvalidResponse, emptyError);

        // 4. All-invalid documents in Glic mode rejects with `lastError`.
        const allInvalidPromise = realProxy.showPicker('tok', 'key', '', true);
        await microtasksFinished();
        capturedCallback({
          action: 'picked',
          docs: [{
            id: 'valid-id',
            mimeType: 'application/vnd.google-apps.folder',
            name: 'Folder',
            type: 'folder',
            sizeBytes: 0,
          }],
        });
        let allInvalidError: unknown = null;
        try {
          await allInvalidPromise;
        } catch (e) {
          allInvalidError = e;
        }
        assertEquals(DrivePickerError.kUnsupportedFileType, allInvalidError);
      });
});
