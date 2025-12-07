# Browser APIS

This component exposes a set of WebUI APIs for the browser functionality. These
APIs may be used by any WebUI clients, so that they do not need to define their
own bespoke APIs when implementing a page.

Note that this directory should only contain the API mojom definitions and
typemapping code. It should not contain implementation code. Implementation code
and binding logic should reside in the embedders (e.g. //chrome). At a high
level, this is the general layering of the APIs.

       ------------
       | Clients  |
       ------------
       | API defs |     <-- contained in this component.
    ------------------  <-- abstraction point.
       | API impl |     <-- resides in the embedder.
       -----------
       | Models   |     <-- embedder specific.
       ------------

The APIs should be agnostic of the underlying embedder and should be flexible enough
to present different underlying models. Clients should not have to modify their code
to work with different embedders.

| Platform          | Implementation | Binder |
--------------------|----------------|--
| Desktop Chrome  | [//chrome/browser/ui/tabs/tab_strip_api](../../chrome/browser/ui/tabs/tab_strip_api) | [chrome/browser/ui/webui_browser/webui_browser_ui.cc](../../chrome/browser/ui/webui_browser/webui_browser_ui.cc)
