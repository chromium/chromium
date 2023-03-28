KeyedService and KeyedServiceFactory together support building a dependency tree
of services that are all keyed off the same object (typically,
content::BrowserContext or web::BrowserState), and whose teardown order is
managed according to the expressed dependency order.

KeyedService is a layered component
(https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design)
to enable it to be shared cleanly on iOS.

This component has the following structure:

- core/: shared code that does not depend on src/content/ or src/ios/
- content/: Code based on the content layer.
- ios/: Code based on src/ios.
