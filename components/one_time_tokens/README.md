# One time tokens

One time tokens are tokens requested by websites to login, create accounts, or
perform actions. Examples include SMS OTPs (one time passwords), email OTPs,
activation links, etc.

This component provides infrastructure to request, receive and provide one time
tokens for autofill to fill them into forms.

The component is limited to providing the infrastructure. Dealing with forms and
rendering suggestions remains the responsibility of components/autofill. As
such, components/autofill depends on components/one_time_tokens and not vice
versa.

## High-level architecture

Initially this will only support SMS OTPs but it is designed to support
multiple types of one time tokens.

```
components/one_time_tokens/
- core/browser/
  - backends/
    - sms/
    - email/
    - [infrastructure to communicate with different backends]
  - permissions/
    - [infrastructure to decide if a site is eligible to receive an OTP]
  - [datastructures]
  - one_time_token_service.{cc,h}
- android/backend/
  - java/
    - [android SMS backend]
```

