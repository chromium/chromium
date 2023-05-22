The optimization guide component contains code for processing hints and machine
learning models received from the remote Chrome Optimization Guide Service.

Optimization Guide is a layered component
(https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design)
to enable it to be easily used on all platforms.

Directory structure:

core/: Shared code that does not depend on src/content/

* Contains the core functionalities to fetch and persist data received from the
  remote Chrome Optimization Guide Service, including but not limited to page
  load metadata and machine learning models.

content/: Driver for the shared code based on the content layer

* Contains the functionality for interpreting the data received from the remote
  Chrome Optimization Guide Service.
