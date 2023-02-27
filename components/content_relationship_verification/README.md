# Content Relationship Verification

This component contains utilities to verify relationships between Web content and the embedder.

## Verification methods

- Digital Asset Links

    Utilities to facilitate making requests to the DigitalAssetLinks API on the Web.

    See [documentation](https://developers.google.com/digital-asset-links/v1/getting-started).


- Response Header Verification

    Class that contains utilities to parse the `X-Embedder-Ancestors`- response header.


## Browser URL Loader Throttle

URL loader throttle that aborts loading of http responses when no relationship between Web content and embedder was verified.