# Quick Start Guide to Using Cronet
Cronet offers its own Java asynchronous API as well as support for the
[java.net.HttpURLConnection] API. This document gives a brief introduction to
using these two Java APIs.

## Using Cronet's Java asynchronous API
First you will need to extend `UrlRequest.Callback` to handle
events during the lifetime of a request. For example:

    class MyCallback extends UrlRequest.Callback {
        @Override
        public void onRedirectReceived(UrlRequest request,
                UrlResponseInfo responseInfo, String newLocationUrl) {
            if (followRedirect) {
                // Let's tell Cronet to follow the redirect!
                request.followRedirect();
            } else {
                // Not worth following the redirect? Abandon the request.
                request.cancel();
            }
        }

        @Override
        public void onResponseStarted(UrlRequest request,
                UrlResponseInfo responseInfo) {
             // Now we have response headers!
             int httpStatusCode = responseInfo.getHttpStatusCode();
             if (httpStatusCode == 200) {
                 // Success! Let's tell Cronet to read the response body.
                 request.read(myBuffer);
             } else if (httpStatusCode == 503) {
                 // Do something. Note that 4XX and 5XX are not considered
                 // errors from Cronet's perspective since the response is
                 // successfully read.
             }
             mResponseHeaders = responseInfo.getAllHeaders();
        }

        @Override
        public void onReadCompleted(UrlRequest request,
                UrlResponseInfo responseInfo, ByteBuffer byteBuffer) {
             // Response body is available.
             doSomethingWithResponseData(byteBuffer);
             // Let's tell Cronet to continue reading the response body or
             // inform us that the response is complete!
             request.read(mBuffer);
        }

        @Override
        public void onSucceeded(UrlRequest request,
                UrlResponseInfo responseInfo) {
             // Request has completed successfully!
        }

        @Override
        public void onFailed(UrlRequest request,
                UrlResponseInfo responseInfo, CronetException error) {
             // Request has failed. responseInfo might be null.
             Log.e("MyCallback", "Request failed. " + error.getMessage());
             // Maybe handle error here. Typical errors include hostname
             // not resolved, connection to server refused, etc.
        }
    }

Make a request like this:

    CronetEngine.Builder engineBuilder = new CronetEngine.Builder(getContext());
    CronetEngine engine = engineBuilder.build();
    Executor executor = Executors.newSingleThreadExecutor();
    MyCallback callback = new MyCallback();
    UrlRequest.Builder requestBuilder = engine.newUrlRequestBuilder(
            "https://www.example.com", callback, executor);
    UrlRequest request = requestBuilder.build();
    request.start();

In the above example, `MyCallback` extends `UrlRequest.Callback`. The request
is started asynchronously. When the response is ready (fully or partially), and
in the event of failures or redirects, `callback`'s methods will be invoked on
`executor`'s thread to inform the client of the request state and/or response
information.

### Downloading Data
When Cronet fetches response headers from the server or gets them from the
cache, `UrlRequest.Callback.onResponseStarted` will be invoked. To read the
response body, the client should call `UrlRequest.read` and supply a
[ByteBuffer] for Cronet to fill. Once a portion or all of
the response body is read, `UrlRequest.Callback.onReadCompleted` will be invoked.
The client may then read and consume the data within `byteBuffer`.
Once the client is ready to consume more data, the client should call
`UrlRequest.read` again. The process continues until
`UrlRequest.Callback.onSucceeded` or `UrlRequest.Callback.onFailed` is invoked,
which signals the completion of the request.

### Uploading Data
    MyUploadDataProvider myUploadDataProvider = new MyUploadDataProvider();
    requestBuilder.setHttpMethod("POST");
    requestBuilder.setUploadDataProvider(myUploadDataProvider, executor);

In the above example, `MyUploadDataProvider` extends `UploadDataProvider`.
When Cronet is ready to send the request body,
`myUploadDataProvider.read(UploadDataSink uploadDataSink,
ByteBuffer byteBuffer)` will be invoked. The client will need to write the
request body into `byteBuffer`. Once the client is done writing into
`byteBuffer`, the client can let Cronet know by calling
`uploadDataSink.onReadSucceeded`. If the request body doesn't fit into
`byteBuffer`, the client can continue writing when `UploadDataProvider.read` is
invoked again. For more details, please see the API reference.

### <a id=configuring-cronet></a> Configuring Cronet
Various configuration options are available via the `CronetEngine.Builder`
object.

Enabling HTTP/2 and QUIC:

- For Example:

        engineBuilder.enableHttp2(true).enableQuic(true);

Controlling the cache:

- Use a 100KiB in-memory cache:

        engineBuilder.enableHttpCache(
                CronetEngine.Builder.HttpCache.IN_MEMORY, 100 * 1024);

- or use a 1MiB disk cache:

        engineBuilder.setStoragePath(storagePathString);
        engineBuilder.enableHttpCache(CronetEngine.Builder.HttpCache.DISK,
                1024 * 1024);

### Debugging
To get more information about how Cronet is processing network
requests, you can start and stop **NetLog** logging by calling
`CronetEngine.startNetLogToFile` and `CronetEngine.stopNetLog`.
Bear in mind that logs may contain sensitive data. You may analyze the
generated log by navigating to [chrome://net-internals#import] using a
Chrome browser.

## Using the java.net.HttpURLConnection API
Cronet offers an implementation of the [java.net.HttpURLConnection] API to make
it easier for apps which rely on this API to use Cronet.
To open individual connections using Cronet's implementation, do the following:

    HttpURLConnection connection =
            (HttpURLConnection)engine.openConnection(url);

To use Cronet's implementation instead of the system's default implementation
for all connections established using `URL.openConnection()` do the following:

    URL.setURLStreamHandlerFactory(engine.createURLStreamHandlerFactory());

Cronet's
HttpURLConnection implementation has some limitations as compared to the system
implementation, including not utilizing the default system HTTP cache (Please
see {@link org.chromium.net.CronetEngine#createURLStreamHandlerFactory} for
more information).
You can configure Cronet and control caching through the
`CronetEngine.Builder` instance, `engineBuilder`
(See [Configuring Cronet](#configuring-cronet) section), before you build the
`CronetEngine` and then call `CronetEngine.createURLStreamHandlerFactory()`.

[ByteBuffer]: https://developer.android.com/reference/java/nio/ByteBuffer.html
[chrome://net-internals#import]: chrome://net-internals#import
[java.net.HttpURLConnection]: https://developer.android.com/reference/java/net/HttpURLConnection.html
