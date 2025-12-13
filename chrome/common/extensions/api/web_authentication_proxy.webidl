// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An object representing a
// <code>PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable()</code>
// call.
dictionary IsUvpaaRequest {
  // An opaque identifier for the request.
  required long requestId;
};

// An object representing a WebAuthn
// <code>navigator.credentials.create()</code> call.
dictionary CreateRequest {
  // An opaque identifier for the request.
  required long requestId;

  // The <code>PublicKeyCredentialCreationOptions</code> passed to
  // <code>navigator.credentials.create()</code>, serialized as a JSON
  // string. The serialization format is compatible with <a
  // href="https://w3c.github.io/webauthn/#sctn-parseCreationOptionsFromJSON">
  // <code>PublicKeyCredential.parseCreationOptionsFromJSON()</code></a>.
  required DOMString requestDetailsJson;
};

// An object representing a WebAuthn <code>navigator.credentials.get()</code>
// call.
dictionary GetRequest {
  // An opaque identifier for the request.
  required long requestId;

  // The <code>PublicKeyCredentialRequestOptions</code> passed to
  // <code>navigator.credentials.get()</code>, serialized as a JSON string.
  // The serialization format is compatible with <a
  // href="https://w3c.github.io/webauthn/#sctn-parseRequestOptionsFromJSON">
  // <code>PublicKeyCredential.parseRequestOptionsFromJSON()</code></a>.
  required DOMString requestDetailsJson;
};

dictionary DOMExceptionDetails {
  required DOMString name;
  required DOMString message;
};

dictionary CreateResponseDetails {
  // The <code>requestId</code> of the <code>CreateRequest</code>.
  required long requestId;

  // The <code>DOMException</code> yielded by the remote request, if any.
  DOMExceptionDetails error;

  // The <code>PublicKeyCredential</code>, yielded by the remote request, if
  // any, serialized as a JSON string by calling
  // href="https://w3c.github.io/webauthn/#dom-publickeycredential-tojson">
  // <code>PublicKeyCredential.toJSON()</code></a>.
  DOMString responseJson;
};

dictionary GetResponseDetails {
  // The <code>requestId</code> of the <code>CreateRequest</code>.
  required long requestId;

  // The <code>DOMException</code> yielded by the remote request, if any.
  DOMExceptionDetails error;

  // The <code>PublicKeyCredential</code>, yielded by the remote request, if
  // any, serialized as a JSON string by calling
  // href="https://w3c.github.io/webauthn/#dom-publickeycredential-tojson">
  // <code>PublicKeyCredential.toJSON()</code></a>.
  DOMString responseJson;
};

dictionary IsUvpaaResponseDetails {
  required long requestId;
  required boolean isUvpaa;
};

// Listener callback for the onRemoteSessionStateChange event.
callback OnRemoteSessionStateChangeListener = undefined ();

interface OnRemoteSessionStateChangeEvent : ExtensionEvent {
  static undefined addListener(OnRemoteSessionStateChangeListener listener);
  static undefined removeListener(OnRemoteSessionStateChangeListener listener);
  static boolean hasListener(OnRemoteSessionStateChangeListener listener);
};

// Listener callback for the onCreateRequest event.
callback OnCreateRequestListener = undefined (CreateRequest requestInfo);

interface OnCreateRequestEvent : ExtensionEvent {
  static undefined addListener(OnCreateRequestListener listener);
  static undefined removeListener(OnCreateRequestListener listener);
  static boolean hasListener(OnCreateRequestListener listener);
};

// Listener callback for the onGetRequest event.
callback OnGetRequestListener = undefined (GetRequest requestInfo);

interface OnGetRequestEvent : ExtensionEvent {
  static undefined addListener(OnGetRequestListener listener);
  static undefined removeListener(OnGetRequestListener listener);
  static boolean hasListener(OnGetRequestListener listener);
};

// Listener callback for the onIsUvpaaRequest event.
callback OnIsUvpaaRequestListener = undefined (IsUvpaaRequest requestInfo);

interface OnIsUvpaaRequestEvent : ExtensionEvent {
  static undefined addListener(OnIsUvpaaRequestListener listener);
  static undefined removeListener(OnIsUvpaaRequestListener listener);
  static boolean hasListener(OnIsUvpaaRequestListener listener);
};

// Listener callback for the onRequestCanceled event.
callback OnRequestCanceledListener = undefined (long requestId);

interface OnRequestCanceledEvent : ExtensionEvent {
  static undefined addListener(OnRequestCanceledListener listener);
  static undefined removeListener(OnRequestCanceledListener listener);
  static boolean hasListener(OnRequestCanceledListener listener);
};

// The <code>chrome.webAuthenticationProxy</code> API lets remote desktop
// software running on a remote host intercept Web Authentication API
// (WebAuthn) requests in order to handle them on a local client.
interface WebAuthenticationProxy {
  // Reports the result of a <code>navigator.credentials.create()</code>
  // call. The extension must call this for every
  // <code>onCreateRequest</code> event it has received, unless the request
  // was canceled (in which case, an <code>onRequestCanceled</code> event is
  // fired).
  [requiredCallback] static Promise<undefined> completeCreateRequest(
      CreateResponseDetails details);

  // Reports the result of a <code>navigator.credentials.get()</code> call.
  // The extension must call this for every <code>onGetRequest</code> event
  // it has received, unless the request was canceled (in which case, an
  // <code>onRequestCanceled</code> event is fired).
  [requiredCallback] static Promise<undefined> completeGetRequest(
      GetResponseDetails details);

  // Reports the result of a
  // <code>PublicKeyCredential.isUserVerifyingPlatformAuthenticator()</code>
  // call. The extension must call this for every
  // <code>onIsUvpaaRequest</code> event it has received.
  [requiredCallback] static Promise<undefined> completeIsUvpaaRequest(
      IsUvpaaResponseDetails details);

  // Makes this extension the active Web Authentication API request proxy.
  //
  // Remote desktop extensions typically call this method after detecting
  // attachment of a remote session to this host. Once this method returns
  // without error, regular processing of WebAuthn requests is suspended, and
  // events from this extension API are raised.
  //
  // This method fails with an error if a different extension is already
  // attached.
  //
  // The attached extension must call <code>detach()</code> once the remote
  // desktop session has ended in order to resume regular WebAuthn request
  // processing. Extensions automatically become detached if they are
  // unloaded.
  //
  // Refer to the <code>onRemoteSessionStateChange</code> event for signaling
  // a change of remote session attachment from a native application to to
  // the (possibly suspended) extension.
  // |PromiseValue|: error
  [requiredCallback] static Promise<DOMString?> attach();

  // Removes this extension from being the active Web Authentication API
  // request proxy.
  //
  // This method is typically called when the extension detects that a remote
  // desktop session was terminated. Once this method returns, the extension
  // ceases to be the active Web Authentication API request proxy.
  //
  // Refer to the <code>onRemoteSessionStateChange</code> event for signaling
  // a change of remote session attachment from a native application to to
  // the (possibly suspended) extension.
  // |PromiseValue|: error
  [requiredCallback] static Promise<DOMString?> detach();

  // A native application associated with this extension can cause this
  // event to be fired by writing to a file with a name equal to the
  // extension's ID in a directory named
  // <code>WebAuthenticationProxyRemoteSessionStateChange</code> inside the
  // <a
  // href="https://chromium.googlesource.com/chromium/src/+/main/docs/user_data_dir.md#default-location">default
  // user data directory</a>
  //
  // The contents of the file should be empty. I.e., it is not necessary to
  // change the contents of the file in order to trigger this event.
  //
  // The native host application may use this event mechanism to signal a
  // possible remote session state change (i.e. from detached to attached, or
  // vice versa) while the extension service worker is possibly suspended. In
  // the handler for this event, the extension can call the
  // <code>attach()</code> or <code>detach()</code> API methods accordingly.
  //
  // The event listener must be registered synchronously at load time.
  static attribute OnRemoteSessionStateChangeEvent onRemoteSessionStateChange;

  // Fires when a WebAuthn <code>navigator.credentials.create()</code> call
  // occurs. The extension must supply a response by calling
  // <code>completeCreateRequest()</code> with the <code>requestId</code> from
  // <code>requestInfo</code>.
  static attribute OnCreateRequestEvent onCreateRequest;

  // Fires when a WebAuthn navigator.credentials.get() call occurs. The
  // extension must supply a response by calling
  // <code>completeGetRequest()</code> with the <code>requestId</code> from
  // <code>requestInfo</code>
  static attribute OnGetRequestEvent onGetRequest;

  // Fires when a
  // <code>PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable()</code>
  // call occurs. The extension must supply a response by calling
  // <code>completeIsUvpaaRequest()</code> with the <code>requestId</code>
  // from <code>requestInfo</code>
  static attribute OnIsUvpaaRequestEvent onIsUvpaaRequest;

  // Fires when a <code>onCreateRequest</code> or <code>onGetRequest</code>
  // event is canceled (because the WebAuthn request was aborted by the
  // caller, or because it timed out). When receiving this event, the
  // extension should cancel processing of the corresponding request on the
  // client side. Extensions cannot complete a request once it has been
  // canceled.
  static attribute OnRequestCanceledEvent onRequestCanceled;
};

partial interface Browser {
  static attribute WebAuthenticationProxy webAuthenticationProxy;
};
