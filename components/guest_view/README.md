# Guest View

Guest View is a mechanism for embedding a page inside another page. Unlike iframes, the embedded content has a distinct FrameTree. See the [FrameTree explainer](/docs/frame_trees.md) for details.

Guest View is the core implementation of several embedding features implemented
in the //extensions and //chrome layers. Notable examples include:

* The [&lt;webview&gt; tag](https://developer.chrome.com/docs/extensions/reference/webviewTag/), which allows Chrome Apps and WebUI pages to embed content from the web.
* [Controlled Frame](/chrome/common/controlled_frame/README.md), which is
implemented on top of the &lt;webview&gt; tag and is available in isolated
contexts. See https://github.com/WICG/controlled-frame for more information.
* MimeHandlerView, which allows certain MIME types to be loaded by an extension. There is a hard coded list of Mime Handler extensions: Chromium's PDF viewer and [QuickOffice](https://chrome.google.com/webstore/detail/gbkeegbaiigmenfmjfclcdgdpimamgkj).

Guest Views are currently implemented using [inner WebContents](https://docs.google.com/document/d/1q1wY2pISRjricWvJrIGSoKKXeVqytkkKpkjgUwe9dS8/edit?usp=sharing). By end of 2024, we plan to [migrate](https://crbug.com/1261928) Guest Views to use [MPArch](https://docs.google.com/document/d/1NginQ8k0w3znuwTiJ5qjYmBKgZDekvEPC22q0I4swxQ/edit?usp=sharing) like other features which nest FrameTrees.

For debugging &lt;webview&gt;, the [Browser Sample](https://chromewebstore.google.com/detail/edggnmnajhcbhlnpjnogkjpghaikidaa) [[source](https://github.com/GoogleChrome/chrome-extensions-samples/tree/main/_archive/apps/samples/webview-samples/browser)] app serves as a basic demo.
