---
name: signin/prefer-access-tokens-over-cookies-for-browser-features
rubric: design
severity: should_fix
---

**Rule.** For browser features communicating with Google endpoints, prefer
authenticating requests with OAuth2 access tokens obtained via
`signin::IdentityManager` (such as `signin::PrimaryAccountAccessTokenFetcher` or
`IdentityManager::CreateAccessTokenFetcherForAccount()`) instead of relying on
Gaia cookies from the cookie jar.

**Why.** Gaia cookies in the cookie jar represent web session state, which may
be stale, cleared by the user, out of sync with the browser's signed-in
account, or contain multiple web accounts without clear attribution. OAuth2
access tokens are explicitly scoped via `signin::OAuthConsumerId`, tied to the
intended Chrome account, and managed independently of web cookies.

## Examples

Bad:

```cpp
auto resource_request = std::make_unique<network::ResourceRequest>();
resource_request->url = GURL(kFeatureEndpointUrl);
// Relies on ambient Gaia cookies in the cookie jar:
resource_request->credentials_mode = network::mojom::CredentialsMode::kInclude;
url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                               traffic_annotation);
```

Good:

```cpp
access_token_fetcher_ =
    std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
        signin::OAuthConsumerId::kMyBrowserFeature, identity_manager_,
        base::BindOnce(&MyFeatureFetcher::OnAccessTokenFetched,
                       base::Unretained(this)),
        signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
        signin::ConsentLevel::kSignin);

void MyFeatureFetcher::OnAccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    return;
  }
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kFeatureEndpointUrl);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", access_token_info.token}));
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
}
```
