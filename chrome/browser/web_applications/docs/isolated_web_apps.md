# Isolated Web Apps

_Isolated Web Apps_ are a proposal for "a way of building applications using web
standard technologies that will have useful security properties unavailable to
normal web pages" ([explainer](https://github.com/WICG/isolated-web-apps)).

Rather than being hosted on live web servers and fetched over HTTPS, these
applications are packaged into Web Bundles, signed by their developer, and
distributed to end-users through one or more distribution methods.

## `isolated-app:` Scheme

Isolated Web Apps are served from an `isolated-app:`
(`webapps::kIsolatedAppScheme`) scheme.
[This explainer](https://github.com/WICG/isolated-web-apps/blob/main/Scheme.md)
provides more details. The scheme is registered in `ChromeContentClient`.

The hostname of a URL with the `isolated-app:` scheme must be a valid _Web
Bundle ID_, which is also detailed in the explainer above.

## Serving Content from Isolated Web Apps

This section provides a brief overview of the classes involved in serving
content from an Isolated Web App:

1. The `IsolatedWebAppURLLoaderFactory` retrieves a request with the
   `isolated-app:` scheme.
2. It creates a new `IsolatedWebAppURLLoader` to handle the request.
3. The `IsolatedWebAppURLLoader` passes the request on to the
   `IsolatedWebAppReaderRegistry::ReadResponse` method.
4. The behavior of `ReadResponse` depends on whether an instance of
   `IsolatedWebAppResponseReader` for the given Signed Web Bundle has already
   been cached.
   - If a `IsolatedWebAppResponseReader` is cached, then that reader is used to
     read the response from the Signed Web Bundle and the response is sent back
     to the loader. This is very fast, since the reader has a map of URLs to
     offsets into the Signed Web Bundle.
   - If a `IsolatedWebAppResponseReader` is not cached, however, the process
     continues and a new reader is created.
5. The Integrity Block is read from the Signed Web Bundle.
6. The validity of the Integrity Block is verified by
   `IsolatedWebAppValidator::ValidateIntegrityBlock`. This includes a check on
   whether the browser trusts the public key(s) used to sign the Web Bundle.
7. If the Integrity Block is valid, then:
   - On non-ChromeOS: The signatures contained in the Isolated Web App are
     verified using `web_package::SignedWebBundleSignatureVerifier`.
   - On ChromeOS: The signatures are only verified during installation, because
     the cryptohome is deemed secure enough to prevent tampering with an already
     installed Isolated Web App.
8. If the signatures are valid, the metadata of the Signed Web Bundle is read
   and validated using `IsolatedWebAppValidator::ValidateMetadata`. This
   includes a check that validates that URLs contained in the Signed Web Bundle
   use the `isolated-app:` scheme, and more.
9. If the metadata is also valid, then the `IsolatedWebAppResponseReader` is
   added to the cache and the response for the given request is read from it.

## Isolated Web Apps vs. Signed Web Bundles

Isolated Web Apps use Signed Web Bundles as their container format. Currently,
Isolated Web Apps are the only use case for Signed Web Bundles. In the future,
other use cases inside Chrome may come up. In preparation for additional use
cases outside of Isolated Web Apps, we strive to maintain a split between the
generic code for Signed Web Bundles, and the code for Isolated Web Apps built on
top of it:

- **Signed Web Bundles**: Parsing and verification of Signed (and unsigned) Web
  Bundles is implemented in `//components/web_package` and
  `//services/data_decoder`.
- **Isolated Web Apps**: Isolated Web Apps are implemented on top of Signed Web
  Bundles. Most code is located in
  `//chrome/browser/web_applications/isolated_web_apps`, but there are also
  other bits and pieces throughout `//content`.

### `web_app::SignedWebBundleReader`

`web_package::WebBundleParser` can be used from the browser process: the CBOR,
Integrity Block and Web Bundle parsing it delegates to is implemented in
`#![forbid(unsafe_code)]` Rust, which satisfies the
[rule of 2](../security/rule-of-2.md) without a sandboxed utility process.

`web_app::SignedWebBundleReader` talks to the parser through
`data_decoder::SafeWebBundleParser`, and hosts the parser's
`web_package::WebBundleParserFactory` in the browser process on a dedicated
`base::MayBlock()` sequence, so that neither parsing nor file I/O happens on the
UI thread.

The `SignedWebBundleReader` is supposed to be a generic reader for Signed Web
Bundles, unrelated to Isolated Web Apps. As such, it does not know anything
about Isolated Web Apps or the `isolated-app:` scheme. Usually, code dealing
with Isolated Web Apps should use the `IsolatedWebAppResponseReader(Factory)` to
read responses from the bundle. It checks the stricter requirements of Signed
Web Bundles when used as Isolated Web Apps. For example, it checks that the URLs
contained in the Signed Web Bundle do not have query parameters or fragments.
